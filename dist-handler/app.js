// app.js
// Seaport handler
//
process.title = 'gh_dist';

var
	redis = require('redis'),
    disco = require('../common').disco,
	_ = require('lodash'),
	redisClient = redis.createClient();

redisClient.on('error', function(err) {
	console.log('Redis client connection errored: ' + err);
});

redisClient.on('ready', function() {
	console.log('Redis client connection is ready');
});


var unregisterForHipache = function(service, cb) {
	var host = (process.env.HOST || 'localhost');
	var key = 'frontend:' + host;
	var self = 'http://' + service.host + ':' + service.port;

	redisClient.lrem(key, 0, self, cb || function() { });
}

var registerForHipache = function(service, cb) {
	var host = (process.env.HOST || 'localhost');
	var key = 'frontend:' + host;
	var self = 'http://' + (service.host || "localhost") + ':' + service.port;

	redisClient.rpush(key, self, cb || function() { });
}

var prepHipacheConfig = function(cb) {
	var host = (process.env.HOST || 'localhost'),
		key = 'frontend:' + host,
		m = redisClient.multi();

	m
		.del(key)
		.rpush(key, 'point-serve')
		.exec(cb);
}

var start = function() {
	prepHipacheConfig(function(err) {
		if (err)
			return console.log('Could not clear initial state');

		var desc = function(service) {
			return service.name + '@' + (service.host || "localhost") + ':' + service.port;
		};

        var watcher = disco.watchForService("ws");
		watcher.on('register', function(service) {
            registerForHipache(service, function(err) {
                if (err) return console.log('Could not register service for hipache: ' + desc(service));
                console.log('hipache registration: ' + desc(service));
            });
		});

        watcher.on('unregister', function(service) {
            console.log('Service is going away: ' + desc(service));
            unregisterForHipache(service, function(err) {
                if (err) return console.log('Unregistering failed: ' + err + ' for ' + desc(service));
            });
        });
    });


		/*
		var pserver = httpProxy.createServer(function(req, res, proxy) {
			var wss = server.query('ws');
			var target = wss[Math.floor(Math.random() * wss.length)];

			console.log('Proxying to: ', desc(target));
			proxy.proxyRequest(req, res, {
				host: target.host,
				port: target.port
			});
		});

		pserver.on('upgrade', function(req, socket, head) {
			var wss = server.query('ws');
			var target = wss[Math.floor(Math.random() * wss.length)];

			console.log('Proxying upgrade to: ', desc(target));
			pserver.proxy.proxyWebSocketRequest(req, socket, head, {
				host: target.host,
				port: target.port
			});
		});

		pserver.listen(80);
		*/
}

process.nextTick(start);
