// disco.js
// Discovery stuff
//


(function(scope) {
    "use strict";

    var uuid = require('node-uuid');
    var freeport = require('freeport');
    var redis = require('redis').createClient();
    var _ = require('lodash');
    var async = require('async');
    var EventEmitter = require('events').EventEmitter;
    var console = require('clim')();

    redis.on('error', function() {
        redis = null;
    });

    var check = function(cbFail, cbPass) {
        if (!redis)
            return cbFail(new Error("Redis not available"));

        cbPass();
    }

    var watchForService = function(name) {
        var current = {};
        var e = new EventEmitter();

        setInterval(function() {
            get(name, function(err, services) {
                // current ids
                var currentIds = _.keys(current);

                // turn the array of services to an object
                var thisServices = {};
                _.forEach(services, function(s) {
                    thisServices[s.id] = s;
                });

                // figure out new IDs
                var thisServicesId = _.keys(thisServices);

                // figure out ids that came in and the ones that left
                var newIds = _.difference(thisServicesId, currentIds);
                var leftIds = _.difference(currentIds, thisServicesId);

                // notify
                _.forEach(newIds, function(i) {
                    e.emit("register", thisServices[i]);
                });

                // notify
                _.forEach(leftIds, function(i) {
                    e.emit("unregister", current[i]);
                });

                // update our local state
                current = thisServices;
            });
        }, 1000);

        return e;
    };

    var register = function(name, cb) {
        var removeOnExit = true; // TODO: this should eventually be configurable

        check(cb, function() {
            // find a free port
            freeport(function(err, port) {
                if (err) return cb(err);

                var id = uuid.v4();
                var val = {
                    name: name,
                    id: id,
                    port: port
                };

                var keyName = 'services/' + name + '/' + id;

                redis.set(keyName, JSON.stringify(val), function() {
                    var aliveC = 0;
                    var ti = setInterval(function() {
                        redis.expire(keyName, 10);
                        aliveC++;
                    }, 5000);

                    val.unregister = function() {
                        console.log('Removing service:', name);
                        clearInterval(ti);
                        redis.del(keyName);
                    };

                    redis.expire(keyName, 10);

                    ["exit", "SIGINT", "SIGTERM"].forEach(function(s) {
                        process.on(s, function() {
                            val.unregister();
                            process.exit();
                        });
                    });

                    cb(null, val);
                });
            });
        });
    };

    var get = function(name, cb) {
        check(cb, function() {
            var pattern = 'services/' + name + '*';
            redis.keys(pattern, function(err, replies) {
                if (err) return cb(err);

                async.map(replies,
                          function(r, cb1) {
                              redis.get(r, function(err, v) {
                                  if (err) return cb1(err);

                                  var service = JSON.parse(v);

                                  // make sure a host field exists
                                  service.host = service.host || "localhost";

                                  cb1(null, service);
                              });
                          }, cb);
            });
        });
    };

    scope.register = register;
    scope.watchForService = watchForService;
    scope.get = get;
})(module.exports);
