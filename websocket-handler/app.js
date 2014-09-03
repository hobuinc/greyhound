// a simple websocket server
//
process.title = 'gh_ws';

var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , express = require('express')
  , seaport = require('seaport')
  , redis = require('redis')

  , web = require('./lib/web')
  , port = (process.env.PORT || 8080)
  , ports = seaport.connect('localhost', 9090)

  , CommandHandler = require('./lib/command-handler').CommandHandler
  , TcpToWs = require('./lib/tcp-ws').TcpToWs
  , streamers = { }

  // TODO Configuration options.
  , softSessionShareMax = 2
  , hardSessionShareMax = 0
  , sessionTimeoutMinutes = 1
  ;

var getTimeSec = function() {
    return Math.round(Date.now() / 1000);
}

var pickSessionHandler = function(exclude, cb) {
    ports.get('sh', function(services) {
        var servicesPruned = { };

        for (var i = 0; i < services.length; ++i) {
            servicesPruned[i] = services[i];
        }

        // Remove exclusions from the service list.
        if (Object.keys(exclude).length) {
            for (var i = 0; i < exclude.length; ++i) {
                var splitExclusion = exclude[i].split(':');
                var excludeHost = splitExclusion[0];
                var excludePort = parseInt(splitExclusion[1]);

                for (var j = 0; j < services.length; ++j) {
                    if (services[j]['host'] === excludeHost &&
                        services[j]['port'] === excludePort) {

                        delete servicesPruned[j];
                    }
                }
            }
        }

        var keys = Object.keys(servicesPruned);

        // Return undefined if all services are excluded.
        if (keys.length) {
            var key = keys[Math.floor(Math.random() * keys.length)];

            var service = servicesPruned[key];
            cb(null, service.host + ':' + service.port);
        }
        else {
            cb(null, undefined);
        }
    });
}

var getDbHandler = function(cb) {
    ports.get('db', function (services) {
        var service = services[0];
        cb(null, service.host + ':' + service.port);
    });
}

var propError = function(missingProp) {
    return new Error('Missing property: ' + missingProp);
}

// REDIS SCHEMA:
//      hash("plAffinities:<pipelineId>"):
//          hash mapping each session handler with an affinity for this
//          pipeline with its number of open client connections to the pipeline
//      hash("plTouched:<pipelineId>"):
//          hash mapping each session handler from above with its last used time
//
//      hash("sessionAffinity"):
//          hash mapping a sessionId to a session handler
//      hash("sessionToPipeline"):
//          hash mapping a sessionId to its pipelineId

var redisClient = redis.createClient();
redisClient.on('error', function(err) {
    console.log('Connection to redis server errored: ' + err);
});

redisClient.on('ready', function() {
    console.log('Connection to redis server established.');
});

var plAffinities = function(pipelineId) {
    return "plAffinities:" + pipelineId;
}

var addToPlAffinities = function(pipelineId, sh, cb) {
    redisClient.hincrby(plAffinities(pipelineId), sh, 1, function(err) {
        return cb(err);
    });
}

var getPlAffinities = function(pipelineId, cb) {
    redisClient.hkeys(plAffinities(pipelineId), function(err, plAffinities) {
        return cb(err, plAffinities);
    });
}

var delFromPlAffinities = function(pipelineId, sh, cb) {
    redisClient.hincrby(plAffinities(pipelineId), sh, -1, function(err, val) {
        if (!val || err) {
            redisClient.hdel(plAffinities(pipelineId), sh, function(err) {
                return cb(err);
            });
        }
        else {
            return cb(err);
        }
    });
}

var getPlCounts = function(pipelineId, cb) {
    redisClient.hgetall(plAffinities(pipelineId), function(err, usage) {
        console.log('COUNT GET', usage);
        if (err) return cb(err);

        var shList = Object.keys(usage);
        var bestSh = shList[0];
        var bestShCount = parseInt(usage[bestSh]);

        console.log(shList);

        for (var curSh in usage) {
            var curCount = parseInt(usage[curSh]);
            if (curCount < bestShCount) {
                bestSh = curSh;
                bestShCount = curCount;
            }
        }

        console.log('best:', bestSh, bestShCount);

        return cb(err, shList, bestSh, bestShCount);
    });
};

/*
var touchedHash = function(pipelineId) {
    return "plTouched:" + pipelineId;
}

var touch = function(pipelineId, sh, cb) {
    redisClient.hset(touchedHash(pipelineId), sh, getTimeSec(), function(err, t) {
        return cb(err, t);
    });
}

var getTouchedTime = function(pipelineId, sh, cb) {
    redisClient.hget(touchedHash(pipelineId), sh, function(err, touched) {
        return cb(err, touched);
    });
}
*/

var redisShHash = "sessionAffinity";
var redisSessionToPl = "sessionToPipeline";

var setSessionAffinity = function(sessionId, pipelineId, sh, cb) {
    redisClient.hset(redisShHash, sessionId, sh, function(err) {
        if (err) return cb(err);

        redisClient.hset(redisSessionToPl, sessionId, pipelineId, function(err) {
            cb(err);
        });
    });
}

var getSessionAffinity = function(sessionId, cb) {
    if (!sessionId) return cb(propError('session'));

    redisClient.hget(redisShHash, sessionId, function(err, sh) {
        if (!err && !sh) {
            err = new Error('Could not retrieve session ' + sessionId);
        }
        else {
            // TODO Update this the last-touched time for this sessionHandler
            // and this pipelineId.  Need to get the pipelineId for this
            // sessionId.
        }

        return cb(err, sh);
    });
}

var delSessionAffinity = function(sessionId, sh, cb) {
    redisClient.hdel(redisShHash, sessionId, function(err) {
        if (err) return cb(err);

        redisClient.hget(redisSessionToPl, sessionId, function(err, plId) {
            if (err) return cb(err);

            redisClient.hdel(redisSessionToPl, sessionId, function(err) {
                if (err) return cb(err);

                delFromPlAffinities(plId, sh, function(err, count) {
                    return cb(err);
                });
            });
        });
    });
}

var addNewPlAffinity = function(pipelineId, sh, cb) {
    // No binding for this pipelineId.  Pick a sessionHandler for it.
    addToPlAffList(pipelineId, sh, function(err) {
        if (err) return cb(err);

        addToPlAffinities(pipelineId, sh, function(err) {
            if (err) {
                delFromPlAffList(pipelineId, sh, function(err) {
                    console.log('Could not delete pipeline affinity');
                    return cb(err);
                });
            }

            return cb(err);
        });
    });
}

var getOrAssignPipelineAffinity = function(pipelineId, cb) {
    getPlAffinities(pipelineId, function(err, plAffinities) {
        if (!plAffinities.length) {
            console.log('Assigning initial pipeline affinity');
            // This pipeline has no affinities.  Assign a random one.
            pickSessionHandler({ }, function(err, sh) {
                if (err) return cb(err);

                addToPlAffinities(pipelineId, sh, function(err) {
                    return cb(err, sh);
                });
            });
        }
        else {
            // At least one session affinity already exists for this pipelineId.
            // If there are too many concurrent sessions for the pipeline on
            // the same session handler, we'll try to offload to a new one.
            // Otherwise we prefer to share.
            getPlCounts(pipelineId, function(err, shList, bestSh, bestShCount) {
                if (err) return cb(err);

                if (bestShCount < softSessionShareMax ||
                    softSessionShareMax <= 0) {

                    console.log('Adding to shared pipeline affinity');

                    addToPlAffinities(pipelineId, bestSh, function(err) {
                        return cb(err, bestSh);
                    });
                }
                else {
                    // Every session handler that currently has this pipeline
                    // open is too overloaded for comfort.  If there are any
                    // other session handlers available, open this pipeline on
                    // one of them.  Otherwise if every session handler
                    // currently has this pipeline active, then add this session
                    // to the session handler that has the smallest client
                    // loading for this pipeline.
                    pickSessionHandler(shList, function(err, sh) {
                        if (err) return cb(err);

                        if (sh) {
                            console.log('Offloading pipelineAff to new SH');
                            addToPlAffinities(pipelineId, sh, function(err) {
                                return cb(err, sh);
                            });
                        }
                        else {
                            if (bestShCount < hardSessionShareMax ||
                                hardSessionShareMax <= 0) {

                                console.log('OVERLOADED soft - assigning anyway');
                                addToPlAffinities(pipelineId, bestSh, function(err) {
                                    return cb(err, bestSh);
                                });
                            }
                            else {
                                console.log('No assign, hard limit exceeded');
                                return "OVERLOADED past hard limit";
                            }
                        }
                    });
                }
            });
        }
    });
}

var queryPipeline = function(pipelineId, cb) {
    getDbHandler(function(err, db) {
        if (err) return cb(err);

        var params = { pipelineId: pipelineId };

        web.get(db, '/retrieve', params, function(err, res) {
                if (err)
                    return cb(err);
                if (!res.hasOwnProperty('pipeline'))
                    return cb('Invalid response from RETRIEVE');
                else
                    return cb(null, res.pipeline);
            }
        );
    });
}

var createSession = function(pipelineId, pipeline, cb) {
    getOrAssignPipelineAffinity(pipelineId, function(err, sessionHandler) {
        if (err) return cb(err);

        var params = {
            pipelineId: pipelineId,
            pipeline: pipeline
        };

        web.post(sessionHandler, '/create', params, function(err, res) {
            if (err)
                return cb(err);
            if (!res.hasOwnProperty('sessionId'))
                return cb('Invalid response from CREATE');

            setSessionAffinity(
                res.sessionId,
                pipelineId,
                sessionHandler,
                function(err) {
                    return cb(err, { session: res.sessionId });
                }
            );
        });
    });
}

process.nextTick(function() {
    // setup a basic express server
    //
    var app = express();
    app.use(function(req, res, next) {
        res.header('X-Powered-By', 'Hobu, Inc.');
        next();
    });

    app.get('/', function(req, res) {
        res.send('Hobu, Inc. point distribution server');
    });

    var server = http.createServer(app);
    var port = ports.register('ws@0.0.1');

    server.listen(port)
    var wss = new WebSocketServer({server: server});

    console.log('Websocket server running on port: ' + port);

    wss.on('connection', function(ws) {
        var handler = new CommandHandler(ws);
        console.log('Got a connection');

        handler.on('put', function(msg, cb) {
            // Validate this pipeline and then hand it to the db-handler.
            if (!msg.hasOwnProperty('pipeline'))
                return cb(propError('put', 'pipeline'));

            var params = { pipeline: msg.pipeline };

            pickSessionHandler(function(err, sh) {
                web.get(sh, '/validate/', params, function(err, res) {
                    if (err || !res.valid) {
                        console.log('PUT - Pipeline validation failed');
                        return cb(err || 'Pipeline is not valid');
                    }

                    getDbHandler(function(err, db) {
                        if (err) return cb(err);

                        web.post(db, '/put', params, function(err, res) {
                            if (err)
                                return cb(err);
                            if (!res.hasOwnProperty('id'))
                                return cb(new Error('Invalid response from PUT'));
                            else
                                cb(null, { pipelineId: res.id });
                        });
                    });
                });
            });
        });

        handler.on('create', function(msg, cb) {
            // Get the stored pipeline corresponding to the requested
            // pipelineId from the db-handler, then defer to the
            // request-handler to create the session.
            if (!msg.hasOwnProperty('pipelineId'))
                return cb(propError('create', 'pipelineId'));

            var pipelineId = msg.pipelineId;

            queryPipeline(pipelineId, function(err, pipeline) {
                if (err)
                    return cb(err);
                else
                    createSession(pipelineId, pipeline, cb);
            });
        });

        handler.on('pointsCount', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('pointsCount', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/pointsCount/' + session, cb);
            });
        });

        handler.on('dimensions', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('dimensions', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/dimensions/' + session, cb);
            });
        });

        handler.on('srs', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('srs', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/srs/' + session, cb);
            });
        });

        handler.on('destroy', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('destroy', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);

                web._delete(sessionHandler, '/' + session, function(err, res) {
                    if (err) return cb(err);

                    delSessionAffinity(session, sessionHandler, function(err) {
                        if (err)
                            console.log('Delete unclean for session', session);

                        cb();
                    });
                });
            });
        });

        handler.on('cancel', function(msg, cb) {
            var session = msg['session'];
            var readId  = msg['readId'];
            if (!session) return cb(propError('cancel', 'session'));
            if (!readId)  return cb(propError('cancel', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);

                var cancel = '/cancel/' + session;
                var params = { readId: readId };

                web.post(sessionHandler, cancel, params, function(err, res) {
                    console.log('post came back', err);
                    var streamer = streamers[session];
                    if (streamer) {
                        console.log(
                            'Cancelled.  Arrived:',
                            streamer.totalArrived,
                            'Sent:',
                            streamer.totalSent);

                        res['numBytes'] = streamer.totalSent;

                        streamer.cancel();

                        delete streamers[session];
                    }

                    return cb(err, res);
                });
            });
        });

        handler.on('read', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('srs', 'session'));

            getSessionAffinity(session, function(err, sessionHandler) {
                if (err) return cb(err);

                if (streamers[session])
                    return cb(new Error(
                            'A "read" request is already executing ' +
                            'for this session'));

                var streamer = new TcpToWs(ws);

                // TODO Should map from session + readId.
                streamers[session] = streamer;

                streamer.on('local-address', function(addr) {
                    console.log('local-bound address for read: ', addr);

                    if (msg.hasOwnProperty('start') &&
                        !_.isNumber(msg['start'])) {
                        return cb(new Error('"start" must be a number'));
                    }

                    if (msg.hasOwnProperty('count') &&
                        !_.isNumber(msg['count'])) {
                        return cb(new Error('"count" must be a number'));
                    }

                    var params = _.extend(addr, msg);
                    var readPath = '/read/' + session;

                    if (params.hasOwnProperty('schema')) {
                        params['schema'] = JSON.stringify(params['schema']);
                    }

                    web.post(
                        sessionHandler,
                        readPath,
                        params,
                        function(err, res) {
                            if (err) {
                                delete streamers[session];
                                console.log(err);
                                streamer.close();
                                return cb(err);
                            }
                            console.log(
                                'TCP-WS - points:',
                                res.numPoints,
                                ', bytes:',
                                res.numBytes);

                            cb(null, res);
                            process.nextTick(function() {
                                streamer.startPushing();
                            });
                        }
                    );
                });

                streamer.on('end', function() {
                    console.log(
                        'Done transmitting point data, r:',
                        streamer.totalArrived,
                        's:',
                        streamer.totalSent);

                    delete streamers[session];
                });

                streamer.start();
            });
        });
    });
});

