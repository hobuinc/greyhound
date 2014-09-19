// a simple websocket server
//
process.title = 'gh_ws';

var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , express = require('express')
  , seaport = require('seaport')

  , web = require('./lib/web')
  , port = (process.env.PORT || 8080)
  , ports = seaport.connect('localhost', 9090)

  , CommandHandler = require('./lib/command-handler').CommandHandler
  , TcpToWs = require('./lib/tcp-ws').TcpToWs
  , Affinity = require('./lib/affinity').Affinity
  , streamers = { }

  , config = require('./config')
  , softSessionShareMax = (config.softSessionShareMax || 16)
  , hardSessionShareMax = (config.hardSessionShareMax || 0)
  , sessionTimeoutMinutes = (config.sessionTimeoutMinutes || 60)
  , expirePeriodSeconds = (config.expirePeriodSeconds || 10);
  ;

var affinity = new Affinity(expirePeriodSeconds, sessionTimeoutMinutes);

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

var propError = function(cmd, missingProp) {
    return new Error(
            'Missing property "' + missingProp + '" in "' + cmd + '" command');
}

var getShCandidate = function(pipelineId, cb) {
    affinity.getPipelineHandlers(pipelineId, function(err, counts) {
        var shList = Object.keys(counts);

        // console.log('COUNTS', counts);

        if (!shList.length) {
            console.log('Assigning initial pipeline affinity');
            // This pipeline has no affinities.  Assign a random one.
            pickSessionHandler({ }, function(err, sh) {
                return cb(err, sh);
            });
        }
        else {
            // At least one session affinity already exists for this pipelineId.
            // If there are too many concurrent sessions for the pipeline on
            // the same session handler, we'll try to offload to a new one.
            // Otherwise we prefer to share.
            var bestSh = shList[0];
            var bestShCount = counts[bestSh];

            for (var curSh in shList) {
                var curCount = counts[curSh];
                if (curCount < bestShCount) {
                    bestSh = curSh;
                    bestShCount = curCount;
                }
            }

            if (bestShCount < softSessionShareMax ||
                softSessionShareMax <= 0) {

                console.log('Sharing!');
                return cb(err, bestSh);
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
                        return cb(err, sh);
                    }
                    else {
                        if (bestShCount < hardSessionShareMax ||
                            hardSessionShareMax <= 0) {

                            console.log('OVERLOADED soft - assigning anyway');
                            return cb(err, bestSh);
                        }
                        else {
                            console.log('No assign - hard limit exceeded');
                            return "OVERLOADED past hard limit";
                        }
                    }
                });
            }
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
        });
    });
}

var createSession = function(pipelineId, pipeline, cb) {
    getShCandidate(pipelineId, function(err, sessionHandler) {
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

            affinity.addSession(
                pipelineId,
                sessionHandler,
                res.sessionId,
                function(err) {
                    return cb(err, { session: res.sessionId });
                }
            );
        });
    });
}

process.nextTick(function() {
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

            pickSessionHandler({ }, function(err, sh) {
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

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/pointsCount/' + session, cb);
            });
        });

        handler.on('schema', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('schema', 'session'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/schema/' + session, cb);
            });
        });

        handler.on('stats', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('stats', 'session'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/stats/' + session, cb);
            });
        });

        handler.on('srs', function(msg, cb) {
            var session = msg['session'];
            if (!session) return cb(propError('srs', 'session'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);
                web.get(sessionHandler, '/srs/' + session, cb);
            });
        });

        handler.on('destroy', function(msg, cb) {
            // Note: this function does not destroy the actual PdalSession that
            // was used for this session.  This just erases the session.  The
            // PdalSession will be destroyed once it has expired, meaning no
            // sessions have used it for a while.
            var session = msg['session'];
            if (!session) return cb(propError('destroy', 'session'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);

                var path = '/sessions/' + session;
                web._delete(sessionHandler, path, function(err, res) {
                    if (err) return cb(err);
                    console.log('Erased session', err, res);

                    affinity.delSession(session, function(err) {
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
            if (!readId)  return cb(propError('cancel', 'readId'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);

                var cancel = '/cancel/' + session;
                var params = { readId: readId };

                web.post(sessionHandler, cancel, params, function(err, res) {
                    console.log('post came back', err);
                    if (streamers.hasOwnProperty(session)) {
                        var streamer = streamers[session][readId];

                        if (streamer) {
                            console.log(
                                'Cancelled.  Arrived:',
                                streamer.totalArrived,
                                'Sent:',
                                streamer.totalSent);

                            res['numBytes'] = streamer.totalSent;

                            streamer.cancel();

                            delete streamers[session][readId];

                            if (Object.keys(streamers[session]) == 0) {
                                delete streamers[session];
                            }
                        }
                    }

                    return cb(err, res);
                });
            });
        });

        handler.on('read', function(msg, cb) {
            var session = msg['session'];
            var readId = 0;
            if (!session) return cb(propError('read', 'session'));

            affinity.getSh(session, function(err, sessionHandler) {
                if (err) return cb(err);

                var streamer = new TcpToWs(ws);

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
                                console.log(err);
                                streamer.close();
                                return cb(err);
                            }

                            readId = res.readId;

                            if (!streamers.hasOwnProperty(session)) {
                                streamers[session] = { };
                            }

                            streamers[session][readId] = streamer;

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

                    delete streamers[session][readId];

                    if (Object.keys(streamers[session]) == 0) {
                        delete streamers[session];
                    }
                });

                streamer.start();
            });
        });
    });
});

