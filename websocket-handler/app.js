// a simple websocket server
//
var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , express = require('express')
  , seaport = require('seaport')
  , redis = require('redis')
  , lruCache = require('lru-cache')

  , web = require('./lib/web')
  , CommandHandler = require('./lib/command-handler').CommandHandler
  , port = (process.env.PORT || 8080)
  , ports = seaport.connect('localhost', 9090)
  , TCPToWS = require('./lib/tcp-ws').TCPToWS;


// setup redis client
var redisClient = redis.createClient();
redisClient.on('error', function(err) {
	console.log('Connection to redis server errored: ' + err);
});

redisClient.on('ready', function() {
	console.log('Connection to redis server established.');
});

var pickRequestHandler = function(cb) {
	ports.get('rh', function(services) {
		var service = services[Math.floor(Math.random() * services.length)];
		cb(null, service.host + ':' + service.port);
	});
}


var affinityCache = lruCache({
	max: 1000,
	maxAge: 1000
});

var setAffinity = function(session, target, cb) {
	redisClient.hset('affinity', session, target, function(err, v) {
		if (err) return cb(err);
		affinityCache.set(session, target);
		cb(null, v);
	})
}

var getAffinity = function(session, cb) {
	var v = affinityCache.get(session);
	if (v !== undefined)
		return cb(null, v);

	redisClient.hget('affinity', session, function(err, val) {
		if (err) return cb(err);

		affinityCache.set(session, val);
		cb(null, val);
	});
}

var deleteAffinity = function(session, cb) {
	redisClient.hdel('affinity', session, function(err, val) {
		if (err) return cb(err);
		affinityCache.del(session);
		cb();
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
			console.log('affinity(' + session + ') = ' + val)
			cb(err, session, val);
		});
	}

	wss.on('headers', function(h) {
		console.log('Headers: ');
		console.log(h);
	});

	wss.on('connection', function(ws) {
		var handler = new CommandHandler(ws);
		console.log('Got a connection');

		handler.on('create', function(msg, cb) {
			pickRequestHandler(function(err, rh) {
				if (err) return cb(err);

				web.post(rh, '/create', function(err, res) {
					if (err) return cb(err);

					setAffinity(res.sessionId, rh, function(err) {
						if (err) {
							// atleast try to clean session
							web._delete(rh, '/' + session, function() { });
							return cb(err);
						}

						// everything went fine, we're good
						cb(null, { session: res.sessionId });
					});
				});
			});
		});

		handler.on('pointsCount', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, rh) {
				if (err) return cb(err);
				web.get(rh, '/pointsCount/' + session, cb);
			});
		});

		handler.on('destroy', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, rh) {
				web._delete(rh, '/' + session, function(err, res) {
					if (err) return cb(err);

					deleteAffinity(session, function(err) {
						if (err) console.log('destroying session, but affinity was not correctly cleared', err);
						cb();
					});
				});
			});
		});

		handler.on('read', function(msg, cb) {
			validateSessionAffinity(msg.session, function(err, session, rh) {
				if (err) return cb(err);

				var streamer = new TCPToWS(ws);
				streamer.on('local-address', function(add) {
					console.log('local-bound address for read: ', add);

					web.post(rh, '/read/' + session, _.extend(add, {
						start: msg.start,
						count: msg.count
					}), function(err, r) {
						if (err) {
							streamer.close();
							return cb(err);
						}
						console.log('TCP-WS: points: ', r.pointsRead, 'bytes:', r.bytesCount);

						cb(null, r);
						process.nextTick(function() {
							streamer.startPushing();
						});
					});
				});

				streamer.on('end', function() {
					console.log('Done transmitting point data');
				});

				streamer.start();
			});
		});
	});
});
