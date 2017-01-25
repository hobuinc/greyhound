var bytes = require('bytes'),
    Bindings = require('../build/Release/session'),
    Session = Bindings.Session,
    totalThreads = require('os').cpus().length,
    threads = Math.max(Math.ceil(totalThreads * 0.6), 4),
    error = (code, message) => ({ code: code, message: message }),

    // resource name -> { session: session, accessed: Date }
    resources = { }
    ;

var clim = require('clim');
clim.getTime = function() {
    var now = new Date();
    return (
        ('0' + now.getHours()).slice(-2) + ':' +
        ('0' + now.getMinutes()).slice(-2) + ':' +
        ('0' + now.getSeconds()).slice(-2) + ':' +
        ('0' + now.getMilliseconds()).slice(-2));
};
clim(console, true);

(function() {
    'use strict';

    var Controller = function(config) {
        this.config = config;

        if (!config.cacheSize) config.cacheSize = '500mb';

        var paths = config.paths;
        var cacheSize = Math.max(bytes('' + config.cacheSize), bytes('500mb'));
        var arbiter = JSON.stringify(config.arbiter || { });
        var timeoutMs = Math.max(config.resourceTimeoutMinutes, 30) * 60 * 1000;

        // We've limited the libuv threadpool size to about half the number of
        // physical CPUs since each of those threads may spawn its own child
        // threads.
        console.log('Using');
        console.log('\tRead paths:', JSON.stringify(this.config.paths));
        console.log('\tCache size:', cacheSize, '(' + bytes(cacheSize) + ')');
        console.log('\tThreads identified:', totalThreads);

        process.env.UV_THREADPOOL_SIZE = threads;
        Bindings.global(paths, cacheSize, arbiter);
        var s = new Session();

        this.getSession = (name, cb) => {
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

            // Call every time, even if this name was found in our session
            // mapping, to ensure that initialization has finished before the
            // session is used.
            try {
                session.create(name, function(err) {
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
        this.getSession(resource, function(err, session) {
            if (err) return cb(err);

            try { return cb(null, JSON.parse(session.info())); }
            catch (e) { return cb(error(500, 'Error parsing info')); }
        });
    };

    Controller.prototype.files = function(resource, query, cb) {
        var search = JSON.stringify(query.search);
        var scale = query.hasOwnProperty('scale') ? query.scale : null;
        var offset = query.hasOwnProperty('offset') ? query.offset : null;

        this.getSession(resource, function(err, session) {
            if (err) return cb(err);

            try {
                var result = JSON.parse(session.files(search, scale, offset));
                return cb(null, result);
            }
            catch (e) { return cb(error(500, e || 'Files error')); }
        });
    };

    Controller.prototype.read = function(resource, query, onInit, onData) {
        var schema = query.schema;
        var filter = query.filter;
        var compress =
            query.hasOwnProperty('compress') &&
            query.compress.toLowerCase() == 'true';
        var scale = query.hasOwnProperty('scale') ? query.scale : null;
        var offset = query.hasOwnProperty('offset') ? query.offset : null;

        // Simplify our query decision tree for later.
        delete query.schema;
        delete query.filter;
        delete query.compress;
        delete query.scale;
        delete query.offset;

        this.getSession(resource, function(err, session) {
            if (err) return onInit(err);

            session.read(
                schema, filter, compress, scale, offset, query, onInit, onData);
        });
    };

    Controller.prototype.hierarchy = function(resource, query, cb) {
        query.vertical =
            query.hasOwnProperty('vertical') &&
            query.vertical.toLowerCase() == 'true';

        this.getSession(resource, (err, session) => {
            if (err) cb(err);
            else session.hierarchy(query, (err, string) => {
                try { return cb(null, JSON.parse(string)); }
                catch (e) { return cb(error(500, e || 'Hierarchy error')); }
            });
        });
    }

    module.exports.Controller = Controller;
})();

