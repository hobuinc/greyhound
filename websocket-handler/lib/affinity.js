var redis = require('redis');
var getTimeSec = function() {
    return Math.round(Date.now() / 1000);
};
var web = require('./web');

(function() {
    "use strict";

    var redisClient = redis.createClient();

var Affinity = function(expirePeriodSec, sessionTimeoutMinutes) {
    redisClient.on('error', function(err) {
        console.log('Connection to redis server errored: ' + err);
    });

    redisClient.on('ready', function() {
        console.log('Connection to redis server established.');
    });

    this.sessionTimeoutMinutes = sessionTimeoutMinutes;

    var destroy = function(pipelineId, sh, cb) {
        web._delete(sh, '/' + pipelineId, function(err, res) {
            return cb(err);
        });
    }

    var eraseExpired = function() {
        var multi = redisClient.multi();

        redisClient.srandmember(pipelineIdsSet(), function(err, plId) {
            if (err || !plId) return;

            // If anything new happens on this pipeline, abort.
            redisClient.watch(pipelineIdsSet(plId));

            redisClient.srandmember(pipelineIdsSet(plId), function(err, sh) {
                if (err || !sh) return;

                var targetSh = pipelineIdsSet(plId, sh);

                // Same for this pipeline/sh combo and its last-touched time.
                redisClient.watch(targetSh);
                redisClient.watch(touchedField(plId, sh));

                redisClient.get(touchedField(plId, sh), function(err, touched) {
                    if (getTimeSec() - touched > sessionTimeoutMinutes * 60) {
                        redisClient.smembers(targetSh, function(err, sessions) {
                            console.log('Expiring sessions:', sessions);

                            // Delete expired sessions from sessions set.
                            // There should be very few sessions here if clients
                            // are properly closing their sessions.

                            // Delete session mappings.
                            for (var i = 0; i < sessions.length; ++i) {
                                multi
                                    .del(sessionIdsSet(), sessions[i])
                                    .del(sessionIdToPipelineId(sessions[i]))
                                    .del(sessionIdToSh(sessions[i]))
                                    ;
                            }

                            multi.del(targetSh)
                            multi.del(touchedField(plId, sh));

                            redisClient.scard(
                                pipelineIdsSet(plId),
                                function(err, n) {
                                    if (n > 1) {
                                        // Other session handlers are still
                                        // active for this pipeline.  Just
                                        // remove this one.
                                        multi.srem(pipelineIdsSet(plId), sh);
                                    }
                                    else {
                                        // Deleting last session handler with
                                        // this pipeline, so fully expire the
                                        // pipelineId.
                                        multi.del(pipelineIdsSet(plId));
                                        multi.srem(pipelineIdsSet(), plId);
                                    }

                                    multi.exec(function(err, replies) {
                                        if (err) console.log(err);

                                        destroy(plId, sh, function(err) {
                                            if (err) console.log(err);
                                        });
                                    });
                                }
                            );
                        });
                    }
                    else {
                        multi.discard();
                    }
                });
            });
        });

        setTimeout(eraseExpired, expirePeriodSec * 1000);
    }

    this.expirePeriodSec = expirePeriodSec;
    setTimeout(eraseExpired, this.expirePeriodSec * 1000);
};

    var pipelineIdsSet = function(pipelineId, sessionHandler) {
        var hash = "pipelineIds";

        if (pipelineId) {
            hash += ":pipelineId:";

            if (sessionHandler) {
                hash += "sessionHandler:" + sessionHandler;
            }
            else {
                hash += pipelineId;
            }
        }

        return hash;
    }

    var touchedField = function(pipelineId, sessionHandler) {
        return pipelineIdsSet(pipelineId, sessionHandler) + ":touchedTime";
    }

    var sessionIdsSet = function(sessionId) {
        var hash = "sessionIds";

        if (sessionId) {
            hash += ":sessionId:" + sessionId;
        }

        return hash;
    }

    var sessionIdToPipelineId = function(sessionId) {
        return sessionIdsSet(sessionId) + ":pipelineId";
    }

    var sessionIdToSh = function(sessionId) {
        return sessionIdsSet(sessionId) + ":sessionHandler";
    }

    Affinity.prototype.addSession = function(
        pipelineId,
        sessionHandler,
        sessionId,
        cb)
    {
        redisClient.multi()
            // Run through the pipelineIds sets.
            .sadd(pipelineIdsSet(), pipelineId)
            .sadd(pipelineIdsSet(pipelineId), sessionHandler)
            .sadd(pipelineIdsSet(pipelineId, sessionHandler), sessionId)
            .set(touchedField(pipelineId, sessionHandler), getTimeSec())
            // Add to the sessionIds sets.
            .sadd(sessionIdsSet(), sessionId)
            .set(sessionIdToPipelineId(sessionId), pipelineId)
            .set(sessionIdToSh(sessionId), sessionHandler)
            // Exec multi command.
            .exec(function(err, replies) {
                return cb(err);
            });
    }

    Affinity.prototype.delSession = function(sessionId, cb) {
        redisClient.get(sessionIdToPipelineId(sessionId), function(err, plId) {
            redisClient.get(sessionIdToSh(sessionId), function(err, sh) {
                redisClient.multi()
                    // Remove this session from pipeline mapping.
                    .srem(pipelineIdsSet(plId, sh), sessionId)
                    // Remove this session from session mapping.
                    .srem(sessionIdsSet(), sessionId)
                    .del(sessionIdToPipelineId(sessionId))
                    .del(sessionIdToSh(sessionId))
                    // Exec multi command.
                    .exec(function(err, replies) {
                        return cb(err);
                    });
            });
        });
    }

    Affinity.prototype.getSh = function(sessionId, cb) {
        redisClient.get(sessionIdToSh(sessionId), function(err, sh) {
            if (!err) {
                redisClient.get(
                    sessionIdToPipelineId(sessionId),
                    function(err, pipelineId) {
                        redisClient.set(
                            touchedField(pipelineId, sh), getTimeSec());

                        if (!err && !sh)
                            err = "Invalid session";

                        return cb(err, sh);
                    }
                );
            }
            else {
                return cb(err);
            }
        });
    }

    Affinity.prototype.getPipelineHandlers = function(pipelineId, cb) {
        redisClient.smembers(pipelineIdsSet(pipelineId), function(err, shList) {
            if (err || !shList)
                return cb(err);

            var multi = redisClient.multi();

            for (var i = 0; i < shList.length; ++i) {
                multi.scard(pipelineIdsSet(pipelineId, shList[i]));
            }

            multi.exec(function(err, replies) {
                if (err) return cb(err);

                var counts = { };

                for (var i = 0; i < shList.length; ++i) {
                    counts[shList[i]] = replies[i];
                }

                return cb(err, counts);
            });
        });
    }

    module.exports.Affinity = Affinity;
})();

