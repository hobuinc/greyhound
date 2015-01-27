var
    _ = require('lodash'),
    crypto = require('crypto'),
    console = require('clim')(),

    disco = require('../common').disco,
    Affinity = require('../common/affinity').Affinity,
    web = require('../common/web'),

    Listener = require('./lib/listener').Listener,

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
    affinity = new Affinity(disco);

(function() {
    'use strict';

    var Controller = function() {
        this.listeners = { }

        var self = this;
        self.propError = function(cmd, missingProp) {
            return new Error(
                    'Missing property "' + missingProp + '" in "' +
                    cmd + '" command');
        }
    }

    Controller.prototype.put = function(pipeline, cb) {
        var self = this;
        console.log("controller::put");
        // Validate this pipeline and then hand it to the db-handler.
        if (!pipeline) return cb(self.propError('put', 'pipeline'));

        affinity.lightestLoad(function(err, sh) {
            if (err) return new Error('No session handler found');

            var params = { pipeline: pipeline };
            web.get(sh, '/validate/', params, function(err, res) {
                if (err || !res.valid) {
                    console.log('PUT - Pipeline validation failed');
                    return cb(err || 'Pipeline is not valid');
                }

                affinity.getDb(function(err, db) {
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

    Controller.prototype.numPoints = function(plId, cb) {
        console.log("controller::numPoints");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/numPoints/' + plId, cb);
        });
    }

    Controller.prototype.schema = function(plId, cb) {
        console.log("controller::schema");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/schema/' + plId, cb);
        });
    }

    Controller.prototype.stats = function(plId, cb) {
        console.log("controller::stats");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/stats/' + plId, cb);
        });
    }

    Controller.prototype.srs = function(plId, cb) {
        console.log("controller::srs");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/srs/' + plId, cb);
        });
    }

    Controller.prototype.fills = function(plId, cb) {
        console.log("controller::fills");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/fills/' + plId, cb);
        });
    }

    Controller.prototype.serialize = function(plId, cb) {
        console.log("controller::serialize");
        affinity.get(plId, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/serialize/' + plId, cb);
        });
    }

    Controller.prototype.cancel = function(plId, readId, cb) {
        var self = this;
        console.log("controller::cancel");
        if (!plId) return cb(self.propError('cancel', 'pipeline'));
        if (!readId)  return cb(self.propError('cancel', 'readId'));

        var res = { cancelled: false };
        var listeners = self.listeners;

        if (listeners.hasOwnProperty(plId)) {
            var listener = listeners[plId][readId];

            if (listener) {
                res.cancelled = true;
                listener.cancel();

                delete listeners[plId][readId];

                if (Object.keys(listeners[plId]).length == 0) {
                    delete listeners[plId];
                }
            }
        }

        return cb(null, res);
    }

    Controller.prototype.read = function(plId, query, onInit, onData, onEnd)
    {
        var self = this;
        var listeners = self.listeners;
        console.log("controller::read");

        affinity.get(plId, function(err, sh) {
            if (err) return onInit(err);

            var listener = new Listener(onData, onEnd);

            listener.listen(function(address) {
                _.extend(query, address);
                var readPath = '/read/' + plId;

                web.post(sh, readPath, query, function(err, res) {
                    if (!err && res.readId) {
                        if (!listeners.hasOwnProperty[plId]) {
                            listeners[plId] = { };
                        }

                        listeners[plId][res.readId] = listener;
                    }

                    onInit(err, res);
                });
            });
        });
    }

    module.exports.Controller = Controller;
})();

