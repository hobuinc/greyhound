var web = require('../../common/web'),
    console = require('clim')(),
    redis = require('redis'),
    _ = require('lodash'),
    async = require('async'),
    now = function() { return Math.round(Date.now() / 1000); };

(function() {
    "use strict";

    var Affinity = function(disco)
    {
        var self = this;

        self.disco = disco;
        self.redis = redis.createClient();

        self.redis.on('error', function(err) {
            console.log('Connection to redis server errored: ' + err);
        });

        self.redis.on('ready', function() {
            console.log('Connection to redis server established.');
        });

        console.log('Watching for session handler departures.');
        var watcher = self.disco.watchForService('sh', 500);

        watcher.on('unregister', function(sh) {
            console.log('Purging SH:', sh);

            self.redis.smembers(shStore(sh), function(err, resources) {
                for (var i = 0; i < resources.length; ++i) {
                    var rs = resources[i];

                    self.redis.del(rsKey(rs), function(err) {
                        if (err) console.error('Error purging SH key:', err);
                    });
                }
            });

            self.redis.del(shStore(sh), function(err) {
                if (err) console.error('Error purging SH hash:', err);
            });
        });
    };

    var shStore = function(sh) {
        return 'sh:' + sh;
    }

    var rsKey = function(rs) {
        return 'rs:' + rs;
    }

    // Extract an IP address from Redis's session handler entry.
    var extractSync = function(sh) {
        if (!sh || !sh.port) return null;
        else return (sh.host || "localhost") + ':' + sh.port;
    }

    var extract = function(sh, cb) {
        var res = extractSync(sh);

        if (!res) return cb('Invalid SH');
        else return cb(null, res);
    }

    Affinity.prototype.add = function(sh, rs, cb) {
        var self = this;

        console.log('Aff add', sh, rs);

        self.redis.multi()
            .sadd(shStore(sh), rs)
            .set (rsKey(rs), sh)
            .exec(function(err, replies) {
                return cb(err);
            })
        ;
    }

    Affinity.prototype.del = function(rs, cb) {
        var self = this;

        console.log('Aff del', rs);

        self.redis.get(rsKey(rs), function(err, sh) {
            self.redis.multi()
                .srem(shStore(sh))
                .del (rsKey(rs))
                .exec(function(err, replies) {
                    return cb(err);
                })
            ;
        });
    }

    Affinity.prototype.get = function(rs, cb) {
        var self = this;

        console.log('Aff get', rs);

        self.redis.get(rsKey(rs), function(err, sh) {
            if (err) return cb(err);

            if (sh) {
                return cb(err, sh);
            }
            else {
                // No affinity for this resource.  Try the session handlers in
                // random order to see if one of them can provide it.
                self.disco.get('sh', function(err, shList) {
                    if (!shList || !shList.length) {
                        return cb('Invalid SH list');
                    }

                    var samples = _.shuffle(shList, shList.length);

                    var exists = function(sh, cb) {
                        async.waterfall([
                            function(cb) {
                                extract(sh, cb);
                            },
                            function(sh, cb) {
                                web.get(sh, '/exists/' + rs, cb);
                            }
                        ],
                        function(err, res) {
                            cb(!err);
                        });
                    };

                    async.detectSeries(samples, exists, function(sh) {
                        if (sh) {
                            extract(sh, function(err, sh) {
                                self.add(sh, rs, function(err) {
                                    return cb(err, sh);
                                });
                            });
                        }
                        else {
                            return cb('No SH found for ' + rs);
                        }
                    });
                });
            }
        });
    }

    module.exports.Affinity = Affinity;
})();

