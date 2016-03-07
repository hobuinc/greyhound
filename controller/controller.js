var error = function(code, message) {
    return { code: code, message: message };
}

var getTimeout = function(raw) {
    if (raw != undefined && raw >= 0) return raw;
    else return 30;
}

var console = require('clim')(),
    querystring = require('querystring'),

    config = (require('../config').cn || { }),
    paths = config.paths,
    output = config.output,

    chunkCacheSize = config.queryLimits.chunkCacheSize,
    threads = Math.ceil(require('os').cpus().length * 1.2),

    Session = require('./build/Release/session').Bindings,

    // resource name -> { session: session, accessed: Date }
    resources = { },
    timeoutMinutes = getTimeout(config.resourceTimeoutMinutes),
    timeoutMs = timeoutMinutes * 60 * 1000
    ;

if (chunkCacheSize < 16) chunkCacheSize = 16;
process.env.UV_THREADPOOL_SIZE = threads;

console.log('Using');
console.log('\tChunk cache size:', chunkCacheSize);
console.log('\tLibuv threadpool size:', threads);

var getSession = function(name, cb) {
    var session;
    var now = new Date();

    if (resources[name]) {
        session = resources[name].session;
    }
    else {
        session = new Session();
        resources[name] = { session: session };
    }

    resources[name].accessed = now;

    // Call every time, even if this name was found in our session mapping, to
    // ensure that initialization has finished before the session is used.
    try {
        session.create(name, paths, chunkCacheSize, function(err) {
            if (err) {
                console.warn(name, 'could not be created');
                delete resources[name];
            }

            return cb(err, session);
        });
    }
    catch (e) {
        delete resources[name];
        console.warn('Caught exception in CREATE:', e);

        return cb(error(500, 'Unknown error during create'));
    }
};

console.log('Read paths:', paths);

(function() {
    'use strict';

    var Controller = function() {
        var clean = () => {
            var now = new Date();

            Object.keys(resources).forEach((name) => {
                if (now - resources[name].accessed > timeoutMs) {
                    console.log('Purging', name);
                    delete resources[name];
                }
            });

            setTimeout(clean, timeoutMs);
        };

        setTimeout(clean, timeoutMs);
    };

    Controller.prototype.info = function(resource, cb) {
        console.log('controller::info');
        getSession(resource, function(err, session) {
            if (err) return cb(err);

            try { return cb(null, JSON.parse(session.info())); }
            catch (e) { return cb(error(500, 'Error parsing info')); }
        });
    };

    Controller.prototype.read = function(resource, query, onInit, onData) {
        console.log('controller::read');

        var schema = query.schema;
        var compress =
            query.hasOwnProperty('compress') &&
            query.compress.toLowerCase() == 'true';
        var normalize =
            query.hasOwnProperty('normalize') &&
            query.normalize.toLowerCase() == 'true';
        var scale = query.hasOwnProperty('scale') ? parseFloat(query.scale) : 0;
        var offset = query.hasOwnProperty('offset') ? query.offset : null;

        // Simplify our query decision tree for later.
        delete query.schema;
        delete query.compress;
        delete query.scale;
        delete query.offset;

        getSession(resource, function(err, session) {
            if (err) return onInit(err);

            var initCb = (err) => onInit(err);
            var dataCb = (err, data, done) => onData(err, data, done);

            session.read(
                schema, compress, scale, offset, query, initCb, dataCb);
        });
    };

    Controller.prototype.hierarchy = function(resource, query, cb) {
        console.log('controller::hierarchy');

        getSession(resource, (err, session) => {
            if (err) cb(err);
            else session.hierarchy(query, (err, string) => {
                try { return cb(null, JSON.parse(string)); }
                catch (e) { return cb(error(500, 'Error parsing hierarchy')); }
            });
        });
    }

    module.exports.Controller = Controller;
})();

