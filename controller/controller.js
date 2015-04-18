var
    _ = require('lodash'),
    console = require('clim')(),

    disco = require('../common').disco,
    web = require('../common/web'),

    Affinity = require('./lib/affinity').Affinity,
    Listener = require('./lib/listener').Listener,

    config = (require('../config').cn || { }),

    resourceTimeoutMinutes =
        (function() {
            if (config.hasOwnProperty('resourceTimeoutMinutes') &&
                config.resourceTimeoutMinutes >= 0) {

                // May be zero to never expire pipelines.
                return config.resourceTimeoutMinutes;
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
                var params = {
                    'address':  address,
                    'query':    query
                };
                var readPath = '/read/' + plId;

                web.post(sh, readPath, params, function(err, res) {
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

