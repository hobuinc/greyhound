// a simple websocket server
//
var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , qs = require('querystring')
  , redis = require('redis')

  , web = require('./lib/web')
  , CommandHandler = require('./lib/command-handler').CommandHandler;

var pickRequestHandler = function(cb) {
	client = redis.createClient();
	client.on('error', cb);
	client.lrange('rh', 0, -1, function(err, v) {
		if (err) return cb(err);
		if (_.isEmpty(v))
			return cb(new Error('There are no request handlers registered'));

		var rh = v[Math.floor(Math.random() * v.length)];

		console.log('Picker request handler: ' + rh);
		cb(null, rh);
	});
}

var checkForValidHandlers = function(cb) {
	var client = redis.createClient();
	client.on('error', cb);
	client.lrange('rh', 0, -1, function(err, res) {
		if (err) return cb(err);
		cb(null, _.size(res));
	});
}

process.nextTick(function() {
	checkForValidHandlers(function(err, count) {
		if (err) return console.log(err);

		console.log('Total registered request handlers: ' + count);
		if (count === 0)
			console.log('No request handlers were found, if none are available, requests will fail');

		var wsSessions = {};
		var affinity = {};
		var nextId = 1;

		var port = (process.env.PORT || 8080)
		var wss = new WebSocketServer({port: port});

		console.log('Websocket server running on port: ' + port);

		wss.on('connection', function(ws) {
			ws.id = nextId ++;

			var handler = new CommandHandler(ws);

			handler.on('create', function(msg, cb) {
				if (wsSessions[ws.id])
					return cb(new Error('A session has already been created'));

				pickRequestHandler(function(err, rh) {
					if (err) return cb(err);

					web.post(rh, '/create', function(err, res) {
						if (err) return cb(err);

						wsSessions[ws.id] = res.sessionId;
						affinity[res.sessionId] = rh;
						cb(); 
					});
				});
			});

			handler.on('pointsCount', function(msg, cb) {
				if (!wsSessions[ws.id])
					return cb(new Error('A session has not been created'));

				var session = wsSessions[ws.id];
				var rh = affinity[session];
				if (!rh)
					return cb(new Error('Could not find affinity for session, routing error'));

				console.log('Points for ', session, ' to rh: ', rh);
				web.get(rh, '/pointsCount/' + session, cb);
			});

			handler.on('destroy', function(msg, cb) {
				if (!wsSessions[ws.id])
					return cb(new Error('A session has not been created'));

				var session = wsSessions[ws.id];
				var rh = affinity[session];
				if (!rh)
					return cb(new Error('Could not find affinity for session, routing error'));


				console.log('Routing ', session, ' to rh: ', rh);
				web._delete(rh, '/' + session, function(err, res) {
					if (err) return cb(err);

					delete affinity[wsSessions[ws.id]]
					delete wsSessions[ws.id];

					cb();
				});
			});
		});
	});
});
