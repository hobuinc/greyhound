var
    _ = require('lodash'),
    console = require('clim')(),

    disco = require('../common').disco,

    Affinity = require('./lib/affinity').Affinity,
    Listener = require('./lib/listener').Listener,
    web = require('./lib/web'),

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
    }

    Controller.prototype.numPoints = function(resource, cb) {
        console.log("controller::numPoints");
        affinity.get(resource, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/numPoints/' + resource, cb);
        });
    }

    Controller.prototype.schema = function(resource, cb) {
        console.log("controller::schema");
        affinity.get(resource, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/schema/' + resource, cb);
        });
    }

    Controller.prototype.stats = function(resource, cb) {
        console.log("controller::stats");
        affinity.get(resource, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/stats/' + resource, cb);
        });
    }

    Controller.prototype.srs = function(resource, cb) {
        console.log("controller::srs");
        affinity.get(resource, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/srs/' + resource, cb);
        });
    }

    Controller.prototype.bounds = function(resource, cb) {
        console.log("controller::bounds");
        affinity.get(resource, function(err, sessionHandler) {
            if (err) return cb(err);
            web.get(sessionHandler, '/bounds/' + resource, cb);
        });
    }

    Controller.prototype.cancel = function(resource, readId, cb) {
        var self = this;
        console.log("controller::cancel");
        if (!resource || !readId) return cb(new Error('Invalid params'));

        var res = { cancelled: false };
        var listeners = self.listeners;

        if (listeners.hasOwnProperty(resource)) {
            var listener = listeners[resource][readId];

            if (listener) {
                res.cancelled = true;
                listener.cancel();

                delete listeners[resource][readId];

                if (Object.keys(listeners[resource]).length == 0) {
                    delete listeners[resource];
                }
            }
        }

        return cb(null, res);
    }

    Controller.prototype.read = function(resource, query, onInit, onData, onEnd)
    {
        var self = this;
        var listeners = self.listeners;
        console.log("controller::read");

        affinity.get(resource, function(err, sh) {
            if (err) return onInit(err);

            var listener = new Listener(onData, onEnd);

            listener.listen(function(address) {
                var params = {
                    'address':  address,
                    'query':    query
                };
                var readPath = '/read/' + resource;

                web.post(sh, readPath, params, function(err, res) {
                    if (!err && res.readId) {
                        if (!listeners.hasOwnProperty[resource]) {
                            listeners[resource] = { };
                        }

                        listeners[resource][res.readId] = listener;
                    }

                    onInit(err, res);
                });
            });
        });
    }

    module.exports.Controller = Controller;
})();

