var
    Promise = require('bluebird'),
    fs = require('fs'),
    http = require('http'),
    https = require('https'),
	path = require('path'),
    express = require('express'),
    session = require('express-session'),
    morgan = require('morgan'),
    bodyParser = require('body-parser'),
    cookieParser = require('cookie-parser'),
    lessMiddleware = require('less-middleware'),
    request = require('request'),
    uuid = require('node-uuid'),

    console = require('clim')(),

    config = (require('../../../config').cn || { }),
    httpConfig = (config ? config.http : { });

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

    if (!config.auth.cacheMinutes.good) config.auth.cacheMinutes.good = 1;
    if (!config.auth.cacheMinutes.bad)  config.auth.cacheMinutes.bad  = 1;
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
        app.use(cookieParser);

        app.use(function(req, res, next) {
            if (config.auth) {
                res.header(
                    'Access-Control-Allow-Headers',
                    'Content-Type, Authorization');
                res.header(
                    'Access-Control-Allow-Methods',
                    'GET, OPTIONS');
            }

            Object.keys(httpConfig.headers).map(function(key) {
                res.header(key, httpConfig.headers[key]);
            });
            res.header('X-powered-by', 'Hobu, Inc.');
            next();
        });

        if (httpConfig.enableStaticServe) this.registerStatic(app);
        this.registerCommands(app);

        if (this.port) {
            http.createServer(app).listen(this.port);
            console.log('HTTP server running on port', this.port);

            if (config.auth) {
                console.warn('Secure cookies disabled since HTTP is enabled');
                console.log('Set http.port to null for secure cookies');
            }
        }

        if (this.securePort) {
            https.createServer(this.creds, app).listen(this.securePort);
            console.log('HTTPS server running on port', this.securePort);
        }
    }

    HttpHandler.prototype.registerStatic = function(app) {
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

    HttpHandler.prototype.registerCommands = function(app) {
        var controller = this.controller;

        if (config.auth) {
            console.log('Proxying auth requests to', config.auth.path);
            var auths = { };

            app.options('*', function(req, res) {
                res.status(200).end();
            });

            app.use('/resource/:resource(*)/:call(info|read)',
                    function(req, res, next)
            {
                var jar = config.auth.signed ? req.signedCookies : req.cookies;
                var id = jar[config.auth.cookieName];

                if (!id) res.status(401).send();

                var resource = req.params.resource;

                if (!auths[id]) auths[id] = { };

                if (!auths[id][resource]) {
                    console.log('Authing', id);
                    var start = new Date();

                    auths[id][resource] = new Promise((resolve, reject) => {
                        var options = {
                            url: config.auth.path + resource,
                            rejectUnauthorized: false
                        };

                        req.pipe(request(options, (err, authResponse) => {
                            if (err) {
                                console.log('Auth proxy err:', err);
                                return reject(500);
                            }

                            var code = authResponse.statusCode;
                            var ok = Math.floor(code / 100) == 2;

                            var time = (new Date() - start) / 1000;

                            console.log('Authed', id, 'in', time, 's:', ok);

                            var timeoutMinutes = ok ?
                                config.auth.cacheMinutes.good :
                                config.auth.cacheMinutes.bad;

                            setTimeout(() => {
                                delete auths[id][resource];
                                if (!Object.keys(auths[id]).length) {
                                    delete auths[id];
                                }
                            }, timeoutMinutes * 60000);

                            if (ok) resolve();
                            else reject(code);
                        }));
                    });
                }

                auths[id][resource].then(() => next())
                .catch((statusCode) => res.status(statusCode).send());
            });
        }

        app.get('/resource/:resource(*)/info', function(req, res) {
            controller.info(req.params.resource, function(err, data) {
                if (err) return res.status(err.code || 500).json(err.message);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource(*)/read', function(req, res) {
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

        app.get('/resource/:resource(*)/hierarchy', function(req, res) {
            var resource = req.params.resource;
            var query = req.query;

            controller.hierarchy(resource, query, (err, data) => {
                if (err) return res.status(err.code || 500).json(err.message);
                else return res.json(data);
            });
        });
    }

    module.exports.HttpHandler = HttpHandler
})();

