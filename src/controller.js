var bytes = require('bytes'),
    production = process.env.NODE_ENV != 'debug',
    buildDir = production ? 'Release' : 'Debug',
    Bindings = require('../build/' + buildDir + '/session'),
    Session = Bindings.Session,
    totalThreads = require('os').cpus().length,
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
        var threadRatio = config.threadRatio || 1.0;
        var threads = Math.max(Math.ceil(totalThreads * threadRatio), 4);
        var cacheSize = Math.max(bytes('' + config.cacheSize), bytes('500mb'));
        var arbiter = config.arbiter || { };
        var timeoutMs = Math.max(config.resourceTimeoutMinutes, 30) * 60 * 1000;

        // We've limited the libuv threadpool size since each of those threads
        // may spawn its own child threads.
        console.log('Using:');
        console.log('\t' + (production ? 'Production' : 'Debug'), 'mode');
        console.log('\tRead paths:', JSON.stringify(this.config.paths));
        console.log('\tCache size:', cacheSize, '(' + bytes(cacheSize) + ')');
        console.log('\tThreads identified:', totalThreads);
        console.log('\tUV pool size:', threads);

        process.env.UV_THREADPOOL_SIZE = threads;
        Bindings.global(paths, cacheSize, arbiter);

        this.getSession = (name, cb) => {
            var session;
            var now = new Date();

            if (resources[name]) {
                session = resources[name].session;
            }
            else {
                session = new Session(name);
                resources[name] = { session: session };
            }

            resources[name].accessed = now;

            // Call every time, even if this name was found in our session
            // mapping, to ensure that initialization has finished before the
            // session is used.
            try {
                session.create(function(err) {
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
        };

        setInterval(clean, timeoutMs);
    };

    Controller.prototype.info = function(resource, cb) {
        this.getSession(resource, function(err, session) {
            if (err) return cb(err);
            else session.info(cb);
        });
    };

    Controller.prototype.files = function(resource, query, cb) {
        this.getSession(resource, function(err, session) {
            if (err) return cb(err);
            else session.files(query, cb);
        });
    };

    Controller.prototype.hierarchy = function(resource, query, cb) {
        this.getSession(resource, (err, session) => {
            if (err) cb(err);
            else session.hierarchy(query, cb);
        });
    }

    Controller.prototype.read = function(resource, query, cb) {
        this.getSession(resource, function(err, session) {
            if (err) return cb(err);
            else session.read(query, cb);
        });
    };

    module.exports.Controller = Controller;
})();

