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

    chunksPerQuery = config.queryLimits.chunksPerQuery,
    chunkCacheSize = config.queryLimits.chunkCacheSize,
    threads = Math.ceil(require('os').cpus().length * 1.2),

    Session = require('./build/Release/session').Bindings,

    resourceIds = { }, // resource name -> session (one to one)
    resourceTimeoutMinutes = getTimeout(config.resourceTimeoutMinutes)
    ;

if (chunksPerQuery < 4) chunksPerQuery = 4;
if (chunkCacheSize < 16) chunkCacheSize = 16;
process.env.UV_THREADPOOL_SIZE = threads;

console.log('Using');
console.log('\tChunks per query:', chunksPerQuery);
console.log('\tChunk cache size:', chunkCacheSize);
console.log('\tLibuv threadpool size:', threads);

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
        session.create(
                name,
                paths,
                chunksPerQuery,
                chunkCacheSize,
                function(err)
        {
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

console.log('Read paths:', paths);

(function() {
    'use strict';

    var Controller = function() { }

    Controller.prototype.info = function(resource, cb) {
        console.log("controller::info");
        getSession(resource, function(err, session) {
            if (err) return cb(err);

            try {
                return cb(null, JSON.parse(session.info()));
            }
            catch (e) {
                return cb(error(500, 'Error parsing info'));
            }
        });
    }

    Controller.prototype.read = function(resource, query, onInit, onData) {
        console.log("controller::read");

        var schema = query.schema;
        var compress = query.hasOwnProperty('compress') && !!query.compress;

        // Simplify our query decision tree for later.
        delete query.schema;
        delete query.compress;

        getSession(resource, function(err, session) {
            if (err) return onInit(err);

            var initCb = function(
                err,
                readId,
                numPoints)
            {
                if (err) {
                    console.warn('controller::read::ERROR:', err);
                    onInit(err);
                }
                else {
                    var props = { readId: readId, numPoints: numPoints };
                    onInit(null, props);
                }
            };

            var dataCb = function(err, data, done) {
                onData(err, data, done);
                if (done) console.log(process.memoryUsage());
            }

            session.read(schema, compress, query, initCb, dataCb);
        });
    }

    module.exports.Controller = Controller;
})();

