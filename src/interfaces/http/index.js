var
    Promise = require('bluebird'),
    _ = require('lodash'),
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
    console = require('clim')();

http.globalAgent.maxSockets = 1024;

var colorCodes = {
    black:      "\x1b[30m",
    red:        "\x1b[31m",
    green:      "\x1b[32m",
    yellow:     "\x1b[33m",
    blue:       "\x1b[34m",
    magenta:    "\x1b[35m",
    cyan:       "\x1b[36m",
    white:      "\x1b[37m"
};

var colors = Object.keys(colorCodes).reduce((p, k) => {
    p[k] = (s) => colorCodes[k] + s + "\x1b[0m";
    return p;
}, { });

(function() {
    'use strict';

    var HttpHandler = function(controller, port, securePort, creds) {
        this.controller = controller;
        this.port = port;
        this.securePort = securePort;
        this.creds = creds;
        this.config = this.controller.config;
        this.httpConfig = this.config.http || { };
        this.auths = { };
    }

    HttpHandler.prototype.start = function(creds) {
        var app = express();

        // app.use(morgan('dev'));
        app.use(bodyParser.json());
        app.use(bodyParser.urlencoded({ extended: true }));
        app.use(cookieParser());

        var self = this;
        app.use(function(req, res, next) {
            if (self.controller.config.auth) {
                res.header(
                    'Access-Control-Allow-Headers',
                    'Content-Type, Authorization');
                res.header(
                    'Access-Control-Allow-Methods',
                    'GET, OPTIONS');
            }

            Object.keys(self.httpConfig.headers).map(function(key) {
                res.header(key, self.httpConfig.headers[key]);
            });
            res.header('X-powered-by', 'Hobu, Inc.');
            next();
        });

        if (this.httpConfig.enableStaticServe) this.registerStatic(app);
        this.registerCommands(app);

        if (this.port) {
            http.createServer(app).listen(this.port);
            console.log('HTTP server running on port', this.port);
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

        if (this.config.auth) {
            console.log('Proxying auth requests to', this.config.auth.path);

            app.options('*', function(req, res) {
                res.status(200).end();
            });

            var self = this;
            app.use('/resource/:resource(*)/:call(info|read|hierarchy)',
                    function(req, res, next)
            {
                var id = req.cookies[self.config.auth.cookieName] || 'anon';

                var resource = req.params.resource;
                if (!self.auths[id]) self.auths[id] = { };

                if (!self.auths[id][resource]) {
                    console.log('Authing', id);
                    var start = new Date();

                    self.auths[id][resource] = new Promise((resolve, reject) =>
                    {
                        var jar = request.jar();
                        var cookie = request.cookie(
                            self.config.auth.cookieName + '=' + id);
                        jar.setCookie(cookie, self.config.auth.path + resource);

                        var options = {
                            url: self.config.auth.path + resource,
                            rejectUnauthorized: false,
                            jar: jar
                        };

                        request(options, (err, authResponse) => {
                            if (err) {
                                console.log('Auth proxy err:', err);
                                return reject(500);
                            }

                            var code = authResponse.statusCode;
                            var ok = Math.floor(code / 100) == 2;

                            var time = (new Date() - start) / 1000;

                            console.log('Authed', id, 'in', time, 's:', ok);

                            var timeoutMinutes = ok ?
                                self.config.auth.cacheMinutes.good :
                                self.config.auth.cacheMinutes.bad;

                            setTimeout(() => {
                                delete self.auths[id][resource];
                                if (!Object.keys(self.auths[id]).length) {
                                    delete self.auths[id];
                                }
                            }, timeoutMinutes * 60000);

                            if (ok) resolve();
                            else reject(code);
                        });
                    });
                }

                self.auths[id][resource].then(() => next())
                .catch((statusCode) => next({
                    code: statusCode,
                    message: 'Authentication error'
                }));
            });
        }

        app.get('/resource/:resource(*)/info', function(req, res, next) {
            controller.info(req.params.resource, function(err, data) {
                if (err) return next(err);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource(*)/read', function(req, res, next) {
            // Terminate query on socket hangup.
            var keepGoing = true;

            var q = _.merge({ }, req.query);

            req.on('close', () => {
                console.log('Socket closed - aborting read');
                keepGoing = false;
            });

            var start = new Date();

            controller.read(
                req.params.resource,
                req.query,
                (err) => {
                    if (err) return next(err);
                    else res.header('Content-Type', 'application/octet-stream');
                },
                (err, data, done) => {
                    if (err) return next(err);
                    if (!data.length) return keepGoing;

                    setImmediate(() => {
                        res.write(data);
                        if (done) {
                            res.end();
                            var end = new Date();

                            console.log(
                                    '/' + req.params.resource + '/read:',
                                    colors.cyan(200),
                                    end - start, 'ms',
                                    'B:', (q.bounds ? q.bounds : '[all]'),
                                    'D: [' + (
                                        q.depthBegin || q.depthEnd ?
                                            q.depthBegin + ', ' + q.depthEnd :
                                        q.depth ? q.depth :
                                        'all'
                                    ) + ')',
                                    'F:', (q.filter ? q.filter : 'none'));
                        }
                    });

                    return keepGoing;
                }
            );
        });

        app.get('/resource/:resource(*)/hierarchy', function(req, res, next) {
            var resource = req.params.resource;
            var query = req.query;

            controller.hierarchy(resource, query, (err, data) => {
                if (err) return next(err);
                else return res.json(data);
            });
        });

        app.use(function(err, req, res, next) {
            console.log('Error handling:', err);
            res.header('Cache-Control', 'public, max-age=10');
            res.status(err.code || 500).json(err.message || 'Unknown error');
        });
    }

    module.exports.HttpHandler = HttpHandler
})();

