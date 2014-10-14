// app.js
// Seaport handler
//
process.title = 'gh_dist';

var
	redis = require('redis'),
    disco = require('../common').disco,
	redisClient = redis.createClient(),
    console = require('clim')();

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
            var name = service.name,
                host = (service.host || "localhost"),
                port = service.port;

            return name + '@' + host + ':' + port;
		};

        var watcher = disco.watchForService("ws");

		watcher.on('register', function(service) {
            registerForHipache(service, function(err) {
                if (err) {
                    return console.log(
                            'Could not register service for hipache: ' +
                            desc(service));
                }

                console.log('hipache registration: ' + desc(service));
            });
		});

        watcher.on('unregister', function(service) {
            console.log('Service is going away: ' + desc(service));
            unregisterForHipache(service, function(err) {
                if (err) {
                    return console.log(
                            'Unregistering failed: ' +
                            err + ' for ' + desc(service));
                }
            });
        });
    });
}

// Register ourselves with disco for status purposes.  This service does not
// listen on a port, instead watching the Redis database, so use -1 as the port
// number to indicate that.
disco.register('dist', -1, function(err, service) {
    if (err) return console.log("Failed to register service:", err);
});

process.nextTick(start);

