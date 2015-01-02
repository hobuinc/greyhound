var redis = require('redis');

// Gets current time in seconds.
var now = function() {
    return Math.round(Date.now() / 1000);
};
var web = require('./web'),
    console = require('clim')();

(function() {
    "use strict";

    var redisClient = redis.createClient();

    var Affinity = function(plTimeoutSec, sessionTimeoutSec)
    {
        redisClient.on('error', function(err) {
            console.log('Connection to redis server errored: ' + err);
        });

        redisClient.on('ready', function() {
            console.log('Connection to redis server established.');
        });

        var destroy = function(sh, plId, cb) {
            web._delete(sh, '/' + plId, function(err, res) {
                return cb(err);
            });
        }

        var expirePeriodSec = 30;
        var self = this;
        var eraseExpired = function() {
            var timestamp = now();

            // Erase undestroyed Greyhound sessions.
            redisClient.srandmember(sessionIdsStore(), function(err, sId) {
                if (err || !sId) return;
                redisClient.smembers(sessionIdsStore(sId), function(err, info) {
                    if (err || !info.length) return;

                    info = JSON.parse(info[0]);
                    var sh   = info.sh;
                    var plId = info.plId;

                    if (!plId || !sh) return;

                    redisClient.smembers(pipelineIdsStore(plId, sh, sId),
                        function(err, t) {
                            if (err || t.length != 1) return;
                            t = t[0];
                            if (timestamp - t > sessionTimeoutSec) {
                                self.delSession(sId, function(err) {
                                    if (err) console.log(err);
                                });
                            }
                        }
                    );
                });
            });

            // If plTimeoutSec == 0, then pipelines will never be deleted.
            if (plTimeoutSec > 0) {
                // Erase idle PDAL sessions.
                var shStore = sessionHandlersStore;
                redisClient.srandmember(shStore(), function(err, sh) {
                    if (err || !sh) return;
                    redisClient.srandmember(shStore(sh), function(err, plId) {
                        if (err || !plId) return;
                        redisClient.smembers(
                            shStore(sh, plId),
                            function(err, t) {
                                if (err || t.length != 1) return;
                                t = t[0];
                                if (timestamp - t > plTimeoutSec) {
                                    self.delPipeline(sh, plId, function(err) {
                                        destroy(sh, plId, function(err) {
                                            if (err) console.log(err);
                                        });
                                    });
                                }
                            }
                        );
                    });
                });
            }

            setTimeout(eraseExpired, expirePeriodSec * 1000);
        }

        setTimeout(eraseExpired, expirePeriodSec * 1000);
    };

    var pipelineIdsStore = function(plId, sh, sId) {
        var hash = 'pipelineIds';

        if (plId) {
            hash += ':' + plId;
            if (sh) {
                hash += ':' + sh;
                if (sId) {
                    hash += ':' + sId;
                }
            }
        }

        return hash;
    }

    var sessionIdsStore = function(sId) {
        var hash = 'sessionIds';

        if (sId) {
            hash += ':' + sId;
        }

        return hash;
    }

    var sessionHandlersStore = function(sh, plId, sId) {
        var hash = 'sessionHandlers';

        if (sh) {
            hash += ':' + sh;
            if (plId) {
                hash += ':' + plId;
            }
        }

        return hash;
    }

    // Adds entries (with pipelineId=12345 and SH=XYZ and sessionId=SID001):
    // PipelineId -> SessionHandlers -> SessionIds -> TouchedTime
    //      pipelineIds -> + 12345
    //      pipelineIds:12345 -> + XYZ
    //      pipelineIds:12345:XYZ -> + SID001
    //      pipelineIds:12345:XYZ:SID001 -> now()
    //
    // SessionId -> SessionHandler AND SessionId -> PipelineId
    //      sessionIds -> + SID001
    //      sessionIds:SID001 -> { sh: 12345, plId: XYZ }
    //
    // SessionHandler -> PipelineIds -> TouchedTime
    //      sessionHandlers -> + XYZ
    //      sessionHandlers:XYZ -> + 12345
    //      sessionHandlers:XYZ:12345 -> + now()
    Affinity.prototype.addSession = function(plId, sh, sId, cb)
    {
        // console.log("AFF: Adding session:", plId, sh, sId);
        var seconds = now();

        redisClient.multi()
            .sadd(pipelineIdsStore(), plId)
            .sadd(pipelineIdsStore(plId), sh)
            .sadd(pipelineIdsStore(plId, sh), sId)
            .sadd(pipelineIdsStore(plId, sh, sId), seconds)
            .sadd(sessionIdsStore(), sId)
            .sadd(sessionIdsStore(sId), JSON.stringify({ sh: sh, plId: plId }))
            .sadd(sessionHandlersStore(), sh)
            .sadd(sessionHandlersStore(sh), plId)
            .sadd(sessionHandlersStore(sh, plId), seconds)
            .exec(function(err, replies) {
                return cb(err);
            })
        ;
    }

    Affinity.prototype.delSession = function(sId, cb) {
        // console.log('AFF: Deleting session', sId);
        redisClient.smembers(sessionIdsStore(sId), function(err, info) {
            info = JSON.parse(info[0]);
            var sh   = info.sh;
            var plId = info.plId;
            redisClient.multi()
                .del (sessionIdsStore(sId))
                .srem(sessionIdsStore(), sId)
                .del (pipelineIdsStore(plId, sh, sId))
                .srem(pipelineIdsStore(plId, sh), sId)
                .exec(function(err, replies) {
                    return cb(err);
                })
            ;
        });
    }

    Affinity.prototype.delPipeline = function(sh, plId, cb) {
        // console.log('AFF: Deleting pipeline', plId, 'from sh', sh);
        redisClient.smembers(pipelineIdsStore(plId, sh), function(err, sIds) {
            if (err) return cb(err);
            for (var i = 0; i < sIds.length; ++i) {
                var sId = sIds[i];
                redisClient.multi()
                    .del (pipelineIdsStore(plId, sh, sId))
                    .srem(sessionIdsStore(), sId)
                    .del (sessionIdsStore(sId))
                    .exec(function(err) {
                        if (err) return cb(err);
                    })
                ;
            }
        });

        redisClient.multi()
            .del (pipelineIdsStore(plId, sh))
            .srem(pipelineIdsStore(plId), sh)
            .del (sessionHandlersStore(sh, plId))
            .srem(sessionHandlersStore(sh), plId)
            .exec(function(err) {
                if (err) return cb(err);
                var plIdsStore = pipelineIdsStore;
                redisClient.smembers(plIdsStore(plId), function(err, shList) {
                    if (err) return cb(err);
                    if (!shList.length) {
                        redisClient.multi()
                            .srem(plIdsStore(), plId)
                            .del (plIdsStore(plId))
                            .exec(function(err) {
                                return cb(err);
                            })
                        ;
                    }
                });
            })
        ;
    }

    Affinity.prototype.getSh = function(sId, cb) {
        console.log('AFF: Getting session handler for', sId);
        redisClient.smembers(sessionIdsStore(sId), function(err, info) {
            console.log('AFF: Got', err, info, info.length);

            if (err) return cb(err);
            else if (!info || !info.length) return cb(new Error('No results'));

            info = JSON.parse(info[0]);
            var plId = info.plId;
            var sh   = info.sh;
            var seconds = now();

            // Update touched time for this session.
            redisClient.multi()
                .del (pipelineIdsStore(plId, sh, sId))
                .sadd(pipelineIdsStore(plId, sh, sId), seconds)
                .del (sessionHandlersStore(sh, plId))
                .sadd(sessionHandlersStore(sh, plId), seconds)
                .exec(function(err) {
                    return cb(err, sh);
                })
            ;
        });
    }

    Affinity.prototype.getPipelineHandlers = function(plId, cb) {
        // console.log('AFF: Getting PL handlers for', plId);
        var store = pipelineIdsStore(plId);
        redisClient.smembers(store, function(err, shList) {
            if (err || !shList) return cb(err);
            var multi = redisClient.multi();
            for (var i = 0; i < shList.length; ++i) {
                multi.scard(pipelineIdsStore(plId, shList[i]));
            }

            multi.exec(function(err, shCounts) {
                if (err) return cb(err);
                var counts = { };
                for (var i = 0; i < shList.length; ++i) {
                    counts[shList[i]] = shCounts[i];
                }

                return cb(err, counts);
            });
        });
    }

    Affinity.prototype.purgeSh = function(sh, cb) {
        // console.log('AFF: Got PURGE request for', sh);
        if (!sh.host || !sh.port) return cb(new Error('Invalid SH', sh));
        sh = sh.host + ':' + sh.port;
        var shStore = sessionHandlersStore;
        var plStore = pipelineIdsStore;
        redisClient.smembers(shStore(sh), function(err, plIds) {
            if (err) return cb(err);

            for (var i = 0; i < plIds.length; ++i) {
                var plId = plIds[i];
                redisClient.smembers(plStore(plId, sh), function(err, sIds) {
                    for (var j = 0; j < sIds.length; ++j) {
                        var sId = sIds[j];

                        redisClient.multi()
                            .del (plStore(plId, sh, sId))
                            .del (sessionIdsStore(sId))
                            .srem(sessionIdsStore(), sId)
                            .exec(function(err) {
                                if (err) console.log('Error purging sId', err);
                            })
                        ;
                    }
                });

                redisClient.multi()
                    .del (plStore(plId, sh))
                    .srem(plStore(plId), sh)
                    .del (shStore(sh, plId))
                    .exec(function(err) {
                        if (err) console.log('Error purging plStore', err);
                    })
                ;
            }
        });

        redisClient.multi()
            .del (shStore(sh))
            .srem(shStore(), sh)
            .exec(function(err) {
                if (err) console.log('Error purging shStore', err);
            })
        ;
    }

    module.exports.Affinity = Affinity;
})();

