var
    Promise = require('bluebird'),
    _ = require('lodash'),
    bytes = require('bytes'),
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
    request = require('request');

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

            Object.keys(self.httpConfig.headers || { }).map(function(key) {
                res.header(key, self.httpConfig.headers[key]);
            });
            res.header('X-powered-by', 'Hobu, Inc.');
            next();
        });

        this.registerStatic(app);
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

        app.get('/resource/:resourceId(*)/static', function(req, res) {
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

        app.use(function(req, res, next) {
            if (req.query) {
                req.query = Object.keys(req.query).reduce((p, k) => {
                    var v = req.query[k];

                    // Boolean values as query params may be strings (not JSON):
                    //      ?param=true
                    if (v == 'true' || v == 'false') p[k] = v == 'true';
                    else {
                        p[k] = JSON.parse(v);

                        // We'll also accept the quoted strings 'true' and
                        // 'false' to be boolean:
                        //      ?param="true"
                        if (p[k] === 'true') p[k] = true;
                        else if (p[k] == 'false') p[k] = false;
                    }

                    return p;
                }, { });
            }
            next();
        });

        app.get('/resource/:resource(*)/info', function(req, res, next) {
            var start = new Date();

            controller.info(req.params.resource, function(err, data) {
                var end = new Date();
                console.log(
                        req.params.resource + '/' +
                        colors.green('info') + ':',
                        colors.magenta(end - start), 'ms');

                if (err) return next(err);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource(*)/files/:search', function(req, res, next)
        {
            var start = new Date();

            // If the search is numeric, convert it to an actual number so it
            // will be JSONified correctly.
            var s = req.params.search;
            if (s.match(/^\d+$/)) s = +s;

            var query = { search: s };

            controller.files(req.params.resource, query, (err, data) => {
                var end = new Date();

                console.log(
                        req.params.resource + '/' +
                        colors.green('file') + ':',
                        colors.magenta(end - start), 'ms',
                        'Q:', query);

                if (err) return next(err);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource(*)/files', function(req, res, next) {
            var start = new Date();

            var q = req.query;

            if (q.search && q.bounds) {
                return next({
                    code: 400,
                    message: 'Cannot specify both "search" and "bounds"'
                });
            }

            controller.files(req.params.resource, q, (err, data) => {
                var end = new Date();

                console.log(
                        req.params.resource + '/' +
                        colors.green('file') + ':',
                        colors.magenta(end - start), 'ms',
                        'Q:', q);

                if (err) return next(err);
                else return res.json(data);
            });
        });

        app.get('/resource/:resource(*)/read', function(req, res, next) {
            // Terminate query on socket hangup.
            var stop = false;

            var q = _.merge({ }, req.query);

            req.on('close', () => {
                console.log('Socket closed - aborting read');
                stop = true;
            });

            var start = new Date();
            var size = 0;
            var first = true;

            controller.read(
                req.params.resource,
                req.query,
                (err, data, done) => {
                    if (err) return next(err);

                    if (first) {
                        res.header('Content-Type', 'application/octet-stream');
                        first = false;
                    }

                    if (!data.length) return true;
                    size += data.length;

                    setImmediate(() => {
                        if (!done) res.write(data);
                        else res.end(data);

                        if (done) {
                            res.end();
                            var end = new Date();

                            console.log(
                                    req.params.resource + '/' +
                                    colors.cyan('read') + ':',
                                    colors.magenta(end - start), 'ms',
                                    'L:', bytes(size - 4),
                                    'D: [' + (
                                        q.depthBegin || q.depthEnd ?
                                            q.depthBegin + ', ' + q.depthEnd :
                                        q.depth ? q.depth :
                                        'all'
                                    ) + ')');
                        }
                    });

                    return stop;
                }
            );
        });

        app.get('/resource/:resource(*)/hierarchy', function(req, res, next) {
            var resource = req.params.resource;
            var q = req.query;

            // If the 'vertical' parameter is passed as a quoted string, make
            // it boolean.
            if (q.vertical && typeof(q.vertical) == 'string') {
                q.vertical = q.vertical == 'true';
            }

            var start = new Date();

            controller.hierarchy(resource, q, (err, data) => {
                if (!err) {
                    var end = new Date();
                    console.log(
                            req.params.resource + '/' +
                            colors.yellow('hier') + ':',
                            colors.magenta(end - start), 'ms',
                            'D: [' + (
                                q.depthBegin || q.depthEnd ?
                                    q.depthBegin + ', ' + q.depthEnd :
                                q.depth ? q.depth : 'all'
                            ) + ')');
                }

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

