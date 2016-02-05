var
    fs = require('fs'),
    http = require('http'),
    https = require('https'),
	path = require('path'),
    express = require('express'),
    session = require('express-session'),
    morgan = require('morgan'),
    bodyParser = require('body-parser'),
    lessMiddleware = require('less-middleware'),
    request = require('request'),
    uuid = require('node-uuid'),

    console = require('clim')(),

    config = (require('../../../config').cn || { }),
    httpConfig = (config ? config.http : { }),
    accessControlString = 'Access-Control-Expose-Headers',
    exposedHeaders = (() => {
        if (httpConfig.headers && httpConfig.headers[accessControlString]) {
            var h = httpConfig.headers[accessControlString];
            delete httpConfig.headers[accessControlString];
            return h;
        }
    })()
    ;

if (config.auth) {
    var maybeAddSlashTo = (s) => s.slice(-1) == '/' ? s : s + '/';
    config.auth.path = maybeAddSlashTo(config.auth.path);

    if (!config.auth.secret) config.auth.secret = uuid.v4();

    if (typeof config.auth.cacheMinutes == 'number') {
        config.auth.cacheMinutes = {
            good: config.auth.cacheMinutes,
            bad: config.auth.cacheMinutes
        };
    }

    config.auth.cacheMinutes.good = Math.max(config.auth.cacheMinutes.good, 1);
    config.auth.cacheMinutes.bad= Math.max(config.auth.cacheMinutes.bad, 1);
}

http.globalAgent.maxSockets = 1024;

(function() {
    'use strict';

    var HttpHandler = function(controller, port, securePort, creds) {
        this.controller = controller;
        this.port = port;
        this.securePort = securePort;
        this.creds = creds;
    }

    HttpHandler.prototype.start = function(creds) {
        var app = express();

        app.use(morgan('dev'));
        app.use(bodyParser.json());
        app.use(bodyParser.urlencoded({ extended: true }));

        // Set the x-powered-by header
        app.use(function(req, res, next) {
            Object.keys(httpConfig.headers).map(function(key) {
                res.header(key, httpConfig.headers[key]);
            });
            res.header('X-powered-by', 'Hobu, Inc.');
            res.header('Access-Control-Expose-Headers', exposedHeaders);
            res.header('Access-Control-Allow-Headers', 'Content-Type');
            next();
        });

        if (httpConfig.enableStaticServe) registerStatic(app);
        registerCommands(this.controller, app);

        if (this.port) {
            var httpServer = http.createServer(app);
            httpServer.listen(this.port);
            console.log('HTTP server running on port', this.port);
        }

        if (this.securePort) {
            var httpsServer = https.createServer(this.creds, app);
            httpsServer.listen(this.securePort);
            console.log('HTTPS server running on port', this.securePort);
        }
    }

    var registerStatic = function(app) {
        app.set('views', __dirname + '/static/views');
        app.set('view engine', 'jade');

        var publicDir = '/static/public';
        app.use(lessMiddleware(path.join(__dirname, publicDir)));
        app.use(express.static(__dirname + publicDir));

        app.get('/ws/:resourceId', function(req, res) {
            res.render('wsView');
        });

        app.get('/http/:resourceId', function(req, res) {
            res.render('httpView');
        });
    };

    var registerCommands = function(controller, app) {
        if (config.auth) {
            var auths = { };

            app.use(session({
                cookie: {
                    path: '/',
                    domain: config.auth.domain,
                    httpOnly: true,
                    secure: true,
                    maxAge: 1000 * 60 * 60 * 24 * 365,
                },
                genid: () => uuid.v4(),
                name: 'greyhound-user',
                resave: false,
                rolling: false,
                saveUninitialized: false,
                secret: config.auth.secret,
                unset: 'keep'
            }));

            app.use(function(req, res, next) {
                if (!req.session.greyhoundId) {
                    req.session.greyhoundId = uuid.v4();
                }

                next();
            });

            app.use('/resource/:resource/*', function(req, res, next) {
                var id = req.session.greyhoundId;
                var resource = req.params.resource;

                if (!auths[id]) auths[id] = { };

                if (!auths[id][resource]) {
                    console.log('Authing', id);
                    var start = new Date();

                    auths[id][resource] = new Promise((resolve, reject) => {
                        var authPath = config.auth.path + resource;
                        req.pipe(request(authPath, (err, authResponse) => {
                            if (err) return reject(500);

                            var code = authResponse.statusCode;
                            var ok = Math.floor(code / 100) == 2;

                            var time = (new Date() - start) / 1000;

                            console.log('Authed', id, 'in', time, 's');

                            var timeoutMs = ok ?
                                config.auth.cacheMinutes.good :
                                config.auth.cacheMinutes.bad;

                            setTimeout(() => {
                                delete auths[id][resource];
                                if (!Object.keys(auths[id]).length) {
                                    delete auths[id];
                                }
                            }, timeoutMs * 60000);

                            if (ok) resolve();
                            else reject(code);
                        }));
                    });
                }

                auths[id][resource].then(() => next())
                .catch((statusCode) => res.status(statusCode).send());
            });
        }

        app.get('/resource/:resource/info', function(req, res) {
            controller.info(req.params.resource, function(err, data) {
                if (err) return res.status(err.code || 500).json(err.message);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource/read', function(req, res) {
            // Terminate query on socket hangup.
            var keepGoing = true;
            req.on('close', () => keepGoing = false);

            controller.read(
                req.params.resource,
                req.query,
                function(err) {
                    if (err) return res.json(err.code || 500, err.message);
                    res.header('Content-Type', 'application/octet-stream');
                },
                function(err, data, done) {
                    if (err) {
                        console.error('Encountered data error');
                        return res.status(err.code || 500).json(err.message);
                    }

                    res.write(data);
                    if (done) res.end();

                    return keepGoing;
                }
            );
        });
    }

    module.exports.HttpHandler = HttpHandler
})();

