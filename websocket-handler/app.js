// a simple websocket server
//
process.title = 'gh_ws';

var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , express = require('express')
  , seaport = require('seaport')
  , redis = require('redis')
  , lruCache = require('lru-cache')

  , web = require('./lib/web')
  , port = (process.env.PORT || 8080)
  , ports = seaport.connect('localhost', 9090)

  , CommandHandler = require('./lib/command-handler').CommandHandler
  , TcpToWs = require('./lib/tcp-ws').TcpToWs;

// setup redis client
var redisClient = redis.createClient();
redisClient.on('error', function(err) {
	console.log('Connection to redis server errored: ' + err);
});

redisClient.on('ready', function() {
	console.log('Connection to redis server established.');
});

var pickSessionHandler = function(cb) {
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

var setAffinity = function(session, target, cb) {
	redisClient.hset('affinity', session, target, cb);
}

var getAffinity = function(session, cb) {
	redisClient.hget('affinity', session, cb);
}

var deleteAffinity = function(session, cb) {
	console.log('Deleting', session);
	redisClient.hdel('affinity', session, cb);
}

var queryPipeline = function(id, cb) {
	getDbHandler(function(err, db) {
		if (err) return cb(err);

		web.get(db, '/retrieve', { pipelineId: id }, function(err, res) {
                if (err)
                    return cb(err);
                else
                    return cb(null, res.pipeline);
			}
        );
	});
}

var createSession = function(pipeline, cb) {
	pickSessionHandler(function(err, sh) {
		if (err) return cb(err);

		web.post(sh, '/create', { pipeline: pipeline }, function(err, res) {
			console.log('Create came back', err, res);

			if (err) return cb(err);

			setAffinity(res.sessionId, sh, function(err) {
				if (err) {
					// at least try to clean session
					web._delete(sh, '/' + session, function() { });

					return cb(err);
				}

				// everything went fine, we're good
				cb(null, { session: res.sessionId });
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

	var validateSessionAffinity = function(session, cb) {
		if (!session)
			return cb(new Error('Session parameter is invalid or missing'));

		getAffinity(session, function(err, val) {
			if (!val)
				return cb(new Error('Affinity not found'));

			console.log('affinity(' + session + ') = ' + val)
			cb(err, session, val);
		});
	}

	wss.on('connection', function(ws) {
		var handler = new CommandHandler(ws);
		console.log('Got a connection');

		handler.on('put', function(msg, cb) {
            // Validate this pipeline and then hand it to the db-handler.
            if (msg.hasOwnProperty('pipeline')) {
                var params = { pipeline: msg.pipeline };

                pickSessionHandler(function(err, sh) {
                    web.get(sh, '/validate/', params, function(err, res) {
                        if (err || !res.valid) {
                            console.log('PUT - Pipeline validation failed');
                            cb(err ? err : 'Pipeline is not valid');
                        }
                        else {
                            getDbHandler(function(err, db) {
                                if (err) {
                                    return cb(err);
                                }

                                web.post(db, '/put', params, function(err, res) {
                                    console.log('PUT came back', err, res);

                                    if (err)
                                        return cb(err);
                                    else
                                        cb(null, { pipelineId: res.id });
                                });
                            });
                        }
                    });
                });

            }
            else {
                return cb(new Error('PUT - Missing property "pipeline"'));
            }
		});

		handler.on('create', function(msg, cb) {
			// Get the stored pipeline corresponding to the requested
			// pipelineId from the db-handler, then defer to the
			// request-handler to create the session.
            if (msg.hasOwnProperty('pipelineId')) {
                queryPipeline(msg.pipelineId, function(err, pipeline) {
                    if (err)
                        return cb(err);
                    else
                        createSession(pipeline, cb);
                });
            }
            else {
                return cb(new Error('Missing property "pipeline"'));
            }
		});

		handler.on('pointsCount', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, sh) {
				if (err) return cb(err);
				web.get(sh, '/pointsCount/' + session, cb);
			});
		});

		handler.on('schema', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, sh) {
				if (err) return cb(err);
				web.get(sh, '/schema/' + session, cb);
			});
		});

		handler.on('srs', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, sh) {
				if (err) return cb(err);
				web.get(sh, '/srs/' + session, cb);
			});
		});


		handler.on('destroy', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, sh) {
				if (err) return cb(err);

				web._delete(sh, '/' + session, function(err, res) {
					if (err) return cb(err);

					deleteAffinity(session, function(err) {
						if (err) {
                            console.log(
                                'destroying session, but affinity was not ' +
                                        'correctly cleared',
                                err);
                        }

						console.log('Completing destroy');
						cb();
					});
				});
			});
		});

		handler.on('read', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, sh) {
				if (err) return cb(err);

				var streamer = new TcpToWs(ws);
				streamer.on('local-address', function(add) {
					console.log('local-bound address for read: ', add);

                    var path = '/read/' + session;
                    var params = _.extend(
                        add,
                        { start: msg.start, count: msg.count, });

					web.post(sh, path, params, function(err, res) {
						if (err) {
                            console.log(err);
							streamer.close();
							return cb(err);
						}
						console.log(
                            'TCP-WS: points: ',
                            res.numPoints,
                            'bytes:',
                            res.numBytes);

						cb(null, res);
						process.nextTick(function() {
							streamer.startPushing();
						});
					});
				});

				streamer.on('end', function() {
					console.log(
                        'Done transmitting point data, r:',
                        streamer.totalArrived,
                        's:',
                        streamer.totalSent);
				});

				streamer.start();
			});
		});
	});
});

