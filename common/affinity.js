var web = require('./web'),
    console = require('clim')(),
    redis = require('redis'),
    now = function() { return Math.round(Date.now() / 1000); };

(function() {
    "use strict";

    var redisClient = redis.createClient();

    var Affinity = function(disco)
    {
        redisClient.on('error', function(err) {
            console.log('Connection to redis server errored: ' + err);
        });

        redisClient.on('ready', function() {
            console.log('Connection to redis server established.');
        });

        var self = this;
        self.disco = disco;

        console.log('Watching for session handler departures.');
        var watcher = self.disco.watchForService('sh', 500);

        watcher.on('unregister', function(sh) {
            console.log('Purging SH:', sh);
            redisClient.smembers(shStore(sh), function(err, plIds) {
                for (var i = 0; i < plIds.length; ++i) {
                    var plId = plIds[i];

                    redisClient.multi()
                        .srem(plStore(), plId)
                        .del (plStore(plId))
                        .exec(function(err) {
                            if (err) console.log('Error purging SH:', err);
                        })
                    ;
                }
            });

            redisClient.del(shStore(sh));
        });
    };

    var plStore = function(plId) {
        var hash = 'pipelines';
        if (plId) { hash += ':' + plId; }
        return hash;
    }

    var shStore = function(sh) {
        return 'handlers:' + sh;
    }

    Affinity.prototype.add = function(plId, sh, cb)
    {
        console.log('Aff add', plId, sh);
        redisClient.multi()
            .sadd(plStore(), plId)
            .set (plStore(plId), sh)
            .sadd(shStore(sh), plId)
            .exec(function(err, replies) {
                return cb(err);
            })
        ;
    }

    Affinity.prototype.del = function(plId, sh, cb) {
        if (!cb) {
            cb = sh;
            sh = null;
        }

        console.log('Aff del', plId);
        var self = this;

        var doDel = function(sh, cb) {
            redisClient.multi()
                .del (plStore(plId))
                .srem(plStore(), plId)
                .srem(shStore(sh), plId)
                .exec(function(err, replies) {
                    return cb(err);
                })
            ;
        }

        if (sh) {
            doDel(sh, cb);
        }
        else {
            self.get(plId, function(err, sh) {
                if (err) return cb(err);
                doDel(sh, cb);
            });
        }
    }

    // Creates the pdalSession for this pipelineId if necessary.
    Affinity.prototype.get = function(plId, cb) {
        console.log('Aff get', plId);
        var self = this;
        if (!plId) return cb('Invalid pipelineId');

        redisClient.get(plStore(plId), function(err, sh) {
            if (err) return cb(err);
            if (sh)  return cb(err, sh);

            // No session handler has this pipelineId activated, so create it.
            self.getDb(function(err, db) {
                if (err) return cb(err);

                // Get the actual contents of the pipeline file.
                self.getPlContents(db, plId, function(err, filename) {
                    if (err) return cb(err);

                    // Request creation of this pipeline.
                    if (!sh) self.lightestLoad(function(err, sh) {
                        if (err) return cb(err);

                        self.add(plId, sh, function(err) {
                            if (err) {
                                console.log('Error adding');
                                self.del(plId);
                                return cb(err);
                            }

                            var params = {
                                pipelineId: plId,
                                filename:   filename,
                            };

                            web.post(sh, '/create', params, function(err, res) {
                                if (err) {
                                    self.del(plId, sh, function(err) {
                                        if (err) {
                                            console.log('Err in del:', err);
                                        }
                                    });
                                }

                                return cb(err, sh);
                            });
                        })
                    });
                });
            });
        });
    }

    Affinity.prototype.getDb = function(cb) {
        console.log('Aff get DB');
        var self = this;
        self.disco.get('db', function (err, services) {
            if (err) return cb(err);
            var service = services[0];
            cb(null, (service.host || "localhost") + ':' + service.port);
        });
    }

    Affinity.prototype.getPlContents = function(db, plId, cb) {
        console.log('Aff get contents', db, plId);
        var params = { pipelineId: plId };

        web.get(db, '/retrieve', params, function(err, res) {
            console.log('    :/retrieve came back, err:', err);
            if (err)
                return cb(err);
            if (!res.hasOwnProperty('pipeline'))
                return cb('Invalid response from RETRIEVE');
            else
                return cb(null, res.pipeline);
        });
    }

    Affinity.prototype.lightestLoad = function(cb) {
        console.log('Aff lightest');
        var self = this;
        self.disco.get('sh', function(err, shList) {
            // TODO
            var sh = shList[0];
            return cb(null, (sh.host || "localhost") + ':' + sh.port);
        });
    }

    module.exports.Affinity = Affinity;
})();

