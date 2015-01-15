var
    _ = require('lodash'),
    crypto = require('crypto'),
    console = require('clim')(),

    disco = require('../common').disco,

    Affinity = require('./lib/affinity').Affinity,
    Listener = require('./lib/listener').Listener,
    web = require('./lib/web'),

    config = (require('../config').cn || { }),

    softSessionShareMax = (config.softSessionShareMax || 16),
    hardSessionShareMax = (config.hardSessionShareMax || 0),
    pipelineTimeoutMinutes =
        (function() {
            if (config.hasOwnProperty('pipelineTimeoutMinutes') &&
                config.pipelineTimeoutMinutes >= 0) {

                // May be zero to never expire pipelines.
                return config.pipelineTimeoutMinutes;
            }
            else {
                return 30;
            }
        })(),
    sessionTimeoutMinutes =
        (function() {
            // Always non-zero.
            if (config.sessionTimeoutMinutes > 0)
                return config.sessionTimeoutMinutes;
            else
                return 5;
        })(),
    affinity = new Affinity(
            pipelineTimeoutMinutes * 60,
            sessionTimeoutMinutes * 60),

    sessionWatch =
        function() {
            var watcher = disco.watchForService('sh', 500);

            watcher.on('unregister', function(service) {
                affinity.purgeSh(service);
            });
        },
    createSessionId =
        function() {
            return crypto.randomBytes(20).toString('hex');
        },
    getTimeSec =
        function() {
            return Math.round(Date.now() / 1000);
        }
    ;

(function() {
    'use strict';

    var Controller = function() {
        // Start watching for removed session handlers.
        process.nextTick(sessionWatch);

        this.listeners = { }

        var self = this;
        self.pickSessionHandler = function(exclude, cb) {
            disco.get('sh', function(err, services) {
                if (err) return cb(err);
                var servicesPruned = { };

                for (var i = 0; i < services.length; ++i) {
                    servicesPruned[i] = services[i];
                }

                // Remove exclusions from the service list.
                if (Object.keys(exclude).length) {
                    for (var i = 0; i < exclude.length; ++i) {
                        var splitExclusion = exclude[i].split(':');
                        var excludeHost = splitExclusion[0];
                        var excludePort = parseInt(splitExclusion[1]);

                        for (var j = 0; j < services.length; ++j) {
                            if (services[j]['host'] === excludeHost &&
                                services[j]['port'] === excludePort) {

                                delete servicesPruned[j];
                            }
                        }
                    }
                }

                var keys = Object.keys(servicesPruned);

                // Return undefined if all services are excluded.
                if (keys.length) {
                    var key = keys[Math.floor(Math.random() * keys.length)];

                    var service = servicesPruned[key];
                    cb(null, service.host + ':' + service.port);
                }
                else {
                    cb(null, undefined);
                }
            });
        }

        self.getDbHandler = function(cb) {
            disco.get('db', function (err, services) {
                if (err) return cb(err);
                var service = services[0];
                cb(null, (service.host || "localhost") + ':' + service.port);
            });
        }

        self.propError = function(cmd, missingProp) {
            return new Error(
                    'Missing property "' + missingProp + '" in "' +
                    cmd + '" command');
        }

        self.queryPipeline = function(pipelineId, cb) {
            console.log("queryPipeline: getting DB handler");
            self.getDbHandler(function(err, db) {
                console.log("    :getDbHandler result:", err, db);
                if (err) return cb(err);

                var params = { pipelineId: pipelineId };

                web.get(db, '/retrieve', params, function(err, res) {
                    console.log('    :/retrieve came back, err:', err);
                    if (err)
                        return cb(err);
                    if (!res.hasOwnProperty('pipeline'))
                        return cb('Invalid response from RETRIEVE');
                    else
                        return cb(null, res.pipeline);
                });
            });
        }

        var getShCandidate = function(pipelineId, cb) {
            affinity.getPipelineHandlers(pipelineId, function(err, counts) {
                var shList = Object.keys(counts);

                console.log('Pipeline affinity counts:', counts);

                if (!shList.length) {
                    console.log('Assigning initial pipeline affinity');
                    // This pipeline has no affinities.  Assign a random one.
                    self.pickSessionHandler({ }, function(err, sh) {
                        return cb(err, sh);
                    });
                }
                else {
                    // At least one session affinity already exists for this
                    // pipelineId.  If there are too many concurrent sessions
                    // for the pipeline on the same session handler, we'll try
                    // to offload to a new one.  Otherwise we prefer to share.
                    var bestSh = shList[0];
                    var bestShCount = counts[bestSh];

                    for (var curSh in shList) {
                        var curCount = counts[curSh];
                        if (curCount < bestShCount) {
                            bestSh = curSh;
                            bestShCount = curCount;
                        }
                    }

                    if (bestShCount < softSessionShareMax ||
                        softSessionShareMax <= 0) {

                        console.log('Sharing!');
                        return cb(err, bestSh);
                    }
                    else {
                        // Every session handler that currently has this
                        // pipeline open is too overloaded for comfort.  If
                        // there are any other session handlers available, open
                        // this pipeline on one of them.  Otherwise if every
                        // session handler currently has this pipeline active,
                        // then add this session to the session handler that
                        // has the smallest client loading for this pipeline.
                        self.pickSessionHandler(shList, function(err, sh) {
                            if (err) return cb(err);

                            if (sh) {
                                console.log('Offloading pipelineAff to new SH');
                                return cb(err, sh);
                            }
                            else {
                                if (bestShCount < hardSessionShareMax ||
                                    hardSessionShareMax <= 0) {

                                    console.log(
                                        'OVERLOADED soft - assigning anyway');

                                    return cb(err, bestSh);
                                }
                                else {
                                    console.log(
                                        'No assign - hard limit exceeded');
                                    return 'OVERLOADED past hard limit';
                                }
                            }
                        });
                    }
                }
            });
        }

        self.createSession = function(pipelineId, pipeline, cb) {
            getShCandidate(pipelineId, function(err, sh) {
                console.log("    :getShCandidate:", err, sh);
                if (err) return cb(err);

                var sId = createSessionId();

                affinity.addSession(pipelineId, sh, sId, function(err) {
                    console.log("    :addSession:", err);

                    if (err) {
                        return affinity.delSession(sId, function() { cb(err); });
                    }

                    var params = {
                        pipelineId: pipelineId,
                        pipeline: pipeline,
                        sessionId: sId
                    };

                    web.post(sh, '/create', params, function(err, res) {
                        console.log("    :/create:", err, res);
                        if (err) {
                            return affinity.delSession(
                                sId,
                                function() { cb(err); });
                        }
                        else {
                            cb(err, { session: sId });
                        }
                    });
                });
            });
        }
    }

    Controller.prototype.put = function(pipeline, cb) {
        var self = this;
        console.log("websocket::handler::put");
        // Validate this pipeline and then hand it to the db-handler.
        if (!pipeline) return cb(self.propError('put', 'pipeline'));

        var params = { pipeline: pipeline };

        self.pickSessionHandler({ }, function(err, sh) {
            if (err) return new Error('No session handler found');

            web.get(sh, '/validate/', params, function(err, res) {
                if (err || !res.valid) {
                    console.log('PUT - Pipeline validation failed');
                    return cb(err || 'Pipeline is not valid');
                }

                self.getDbHandler(function(err, db) {
                    if (err) return cb(err);

                    web.post(db, '/put', params, function(err, res) {
                        if (err)
                            return cb(err);
                        if (!res.hasOwnProperty('id'))
                            return cb(new Error(
                                    'Invalid response from PUT'));
                        else
                            cb(null, { pipelineId: res.id });
                    });
                });
            });
        });
    }

    Controller.prototype.create = function(pipelineId, cb) {
        var self = this;
        console.log("websocket::handler::create");
        // Get the stored pipeline corresponding to the requested
        // pipelineId from the db-handler, then defer to the
        // request-handler to create the session.
        if (!pipelineId)
            return cb(self.propError('create', 'pipelineId'));

        console.log("    :querying pipeline", pipelineId);
        self.queryPipeline(pipelineId, function(err, pipeline) {
            if (err)
                return cb(err);
            else
                self.createSession(pipelineId, pipeline, cb);
        });
    }

    Controller.prototype.numPoints = function(session, cb) {
        console.log("websocket::handler::numPoints");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/numPoints/' + session, cb);
        });
    }

    Controller.prototype.schema = function(session, cb) {
        console.log("websocket::handler::schema");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/schema/' + session, cb);
        });
    }

    Controller.prototype.stats = function(session, cb) {
        console.log("websocket::handler::stats");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/stats/' + session, cb);
        });
    }

    Controller.prototype.srs = function(session, cb) {
        console.log("websocket::handler::srs");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/srs/' + session, cb);
        });
    }

    Controller.prototype.fills = function(session, cb) {
        console.log("websocket::handler::fills");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/fills/' + session, cb);
        });
    }

    Controller.prototype.serialize = function(session, cb) {
        console.log("websocket::handler::serialize");
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/serialize/' + session, cb);
        });
    }

    Controller.prototype.destroy = function(session, cb) {
        console.log("websocket::handler::destroy");

        // Note: this function does not destroy the actual PdalSession
        // that was used for this client session.  This just erases the
        // client session mapping.  The PdalSession will be destroyed
        // once it has expired, meaning that no client sessions have
        // used it for a while.
        affinity.getSh(session, function(err, sessionHandler) {
            if (err) return cb(err);

            var path = '/sessions/' + session;
            web._delete(sessionHandler, path, function(err, res) {
                if (err) return cb(err);
                console.log('Erased session', err, res);

                affinity.delSession(session, function(err) {
                    if (err)
                        console.log(
                            'Delete unclean for session',
                            session);

                    cb();
                });
            });
        });
    }

    Controller.prototype.cancel = function(session, readId, cb) {
        var self = this;
        console.log("websocket::handler::cancel");
        if (!session) return cb(self.propError('cancel', 'session'));
        if (!readId)  return cb(self.propError('cancel', 'readId'));

        var res = { cancelled: false };
        var listeners = self.listeners;

        if (listeners.hasOwnProperty(session)) {
            var listener = listeners[session][readId];

            if (listener) {
                res.cancelled = true;
                listener.cancel();

                delete listeners[session][readId];

                if (Object.keys(listeners[session]).length == 0) {
                    delete listeners[session];
                }
            }
        }

        return cb(null, res);
    }

    Controller.prototype.read = function(session, params, onInit, onData, onEnd)
    {
        var self = this;
        var listeners = self.listeners;
        console.log("websocket::handler::read");

        var summary = params.summary;
        if (params.hasOwnProperty('summary')) delete params.summary;

        affinity.getSh(session, function(err, sh) {
            if (err) return onInit(err);

            var listener = new Listener(onData, onEnd);

            listener.listen(function(address) {
                _.extend(params, address);
                var readPath = '/read/' + session;

                web.post(sh, readPath, params, function(err, res) {
                    if (!err && res.readId) {
                        if (!listeners.hasOwnProperty[session]) {
                            listeners[session] = { };
                        }

                        listeners[session][res.readId] = listener;
                    }

                    onInit(err, res);
                });
            });
        });
    }

    module.exports.Controller = Controller;
})();

