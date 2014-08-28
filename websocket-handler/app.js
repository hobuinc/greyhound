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
  , streamers = { };

// setup redis client
var redisClient = redis.createClient();
redisClient.on('error', function(err) {
    console.log('Connection to redis server errored: ' + err);
});

redisClient.on('ready', function() {
    console.log('Connection to redis server established.');
});

var pickSessionHandler = function(cb) {
    // TODO Check pipeline affinity.  Only pick random if empty.

    ports.get('sh', function(services) {
        var service = services[Math.floor(Math.random() * services.length)];
        cb(null, service.host + ':' + service.port);
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


// TODO For now, pipeline affinity is a one to one relationship, meaning that
// a single pipeline ID maps to a single session handler.  Therefore all
// session affinities using the same underlying pipeline will map to the same
// session handler.
//
// However if a pipeline has many concurrent users, we want to be able to use
// multiple session handlers for the same pipeline.
var getOrAssignPipelineAffinity = function(pipelineId, cb) {
    var hash = 'pipelineAffinity';

    redisClient.hget(hash, pipelineId, function(err, sh) {
        if (!sh) {
            pickSessionHandler(function(err, sh_) {
                redisClient.hset(hash, pipelineId, sh_, function(err) {
                    cb(err, sh_);
                });
            });
        }
        else {
            cb(err, sh);
        }
    });
}

var deletePipelineAffinity = function(pipelineId, cb) {
    console.log('Deleting pipeline affinity for pipeline', pipelineId);
    redisClient.hdel('pipelineAffinity', pipelineId, cb);
}

var setSessionAffinity = function(sessionId, sh, cb) {
    redisClient.hset('sessionAffinity', sessionId, sh, cb);
}

var getSessionAffinity = function(sessionId, cb) {
    if (!sessionId) return cb(propError('session'));

    redisClient.hget('sessionAffinity', sessionId, function(err, sh) {
        if (!sh) err = new Error('Could not retrieve session ' + sessionId);
        return cb(err, sh);
    });
}

var deleteSessionAffinity = function(sessionId, cb) {
    console.log('Deleting session affinity for session', sessionId);
    redisClient.hdel('sessionAffinity', sessionId, cb);
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

            setSessionAffinity(res.sessionId, sessionHandler, function(err) {
                return cb(err, { session: res.sessionId });
            });
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

                    deleteSessionAffinity(session, function(err) {
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

