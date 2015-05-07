var error = function(code, message) {
    return { code: code, message: message };
}

var getTimeout = function(raw) {
    if (raw != undefined && raw >= 0) return raw;
    else return 30;
}

var getSchema = function(json, cb) {
    if (!json) {
        return cb(null, []);
    }
    else {
        try {
            return cb(null, JSON.parse(json));
        }
        catch (e) {
            return cb(error(400, 'Invalid schema'));
        }
    }
}

var console = require('clim')(),

    config = (require('../config').cn || { }),
    inputs = config.inputs,
    output = config.output,
    aws = config.aws,

    Session = require('./build/Release/session').Bindings,

    resourceIds = { }, // resource name -> session (one to one)
    resourceTimeoutMinutes = getTimeout(config.resourceTimeoutMinutes)
    ;

var getSession = function(name, cb) {
    var session;

    if (resourceIds[name]) {
        session = resourceIds[name];
    }
    else {
        session = new Session();
        resourceIds[name] = session;
    }

    // Call every time, even if this name was found in our session mapping, to
    // ensure that initialization has finished before the session is used.
    try {
        console.log('Calling create on', name);
        session.create(name, aws, inputs, output, function(err) {
            if (err) {
                console.warn(name, 'could not be created');
                delete resourceIds[name];
            }

            return cb(err, session);
        });
    }
    catch (e) {
        delete resourceIds[name];
        console.warn('Caught exception in CREATE:', e);

        return cb(error(500, 'Unknown error during create'));
    }
};

console.log('Read paths:', inputs);
console.log('Write path:', output);
if (aws)
    console.log('S3 credentials found');
else
    console.log('S3 disabled - no credentials supplied');

(function() {
    'use strict';

    var Controller = function() { }

    Controller.prototype.numPoints = function(resource, cb) {
        console.log("controller::numPoints");
        getSession(resource, function(err, session) {
            if (err) return cb(err);
            else return cb(null, session.getNumPoints());
        });
    }

    Controller.prototype.schema = function(resource, cb) {
        console.log("controller::schema");
        getSession(resource, function(err, session) {
            if (err) return cb(err);
            else return cb(null, session.getSchema());
        });
    }

    Controller.prototype.stats = function(resource, cb) {
        console.log("controller::stats");
        getSession(resource, function(err, session) {
            if (err) return cb(err);

            try {
                var stats = JSON.parse(session.getStats());
                return cb(null, stats);
            }
            catch (e) {
                console.warn('Invalid stats -', resource);
                return cb(error(500, 'Stats could not be parsed as JSON'));
            }
        });
    }

    Controller.prototype.srs = function(resource, cb) {
        console.log("controller::srs");
        getSession(resource, function(err, session) {
            if (err) return cb(err);
            else return cb(null, session.getSrs());
        });
    }

    Controller.prototype.bounds = function(resource, cb) {
        console.log("controller::bounds");
        getSession(resource, function(err, session) {
            if (err) return cb(err);
            else return cb(null, session.getBounds());
        });
    }

    Controller.prototype.cancel = function(resource, readId, cb) {
        // TODO
        getSession(resource, function(err, session) {
            if (err) return cb(err);
            else return cb(null, session.cancel(readId));
        });
    }

    Controller.prototype.read = function(resource, query, onInit, onData) {
        console.log("controller::read");

        getSchema(query.schema, function(err, schema) {
            if (err) return cb(err);

            var compress = query.hasOwnProperty('compress') && !!query.compress;

            // Simplify our query decision tree for later.
            delete query.schema;
            delete query.compress;

            getSession(resource, function(err, session) {
                if (err) return cb(err);

                var readCb = function(
                    err,
                    readId,
                    numPoints,
                    numBytes,
                    rasterMeta)
                {
                    if (err) {
                        console.warn('controller::read::ERROR:', err);
                        onInit(err);
                    }
                    else {
                        var props = {
                            readId: readId,
                            numPoints: numPoints,
                            numBytes: numBytes,
                        };

                        if (rasterMeta) {
                            props.rasterMeta = rasterMeta;
                        }

                        onInit(null, props);
                    }
                };

                var dataCb = function(err, data, done) {
                    onData(err, data, done);
                }

                session.read(schema, compress, query, readCb, dataCb);
            });
        });
    }

    module.exports.Controller = Controller;
})();

