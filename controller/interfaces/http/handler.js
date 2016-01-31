var
    fs = require('fs'),
    http = require('http'),
    https = require('https'),
	path = require('path'),
    express = require('express'),
    bodyParser = require('body-parser'),
    methodOverride = require('method-override'),
    lessMiddleware = require('less-middleware'),

    console = require('clim')(),

    controllerConfig = (require('../../../config').cn || { }),
    httpConfig = (controllerConfig ? controllerConfig.http : { }),
    accessControlString = 'Access-Control-Expose-Headers',
    exposedHeaders = (() => {
        if (httpConfig.headers && httpConfig.headers[accessControlString]) {
            var h = httpConfig.headers[accessControlString];
            delete httpConfig.headers[accessControlString];
            return h;
        }
    })();
    ;

http.globalAgent.maxSockets = 1024;

(function() {
    'use strict';

    var HttpHandler = function(controller, port) {
        this.controller = controller;
        this.port = port;
    }

    HttpHandler.prototype.start = function(creds) {
        var app = express();

        app.use(express.logger('dev'));
        app.use(bodyParser.json());
        app.use(bodyParser.urlencoded({ extended: true }));
        app.use(methodOverride());

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

        if (app.get('env') == 'development') {
            app.use(express.errorHandler());
        }

        app.use(app.router);

        if (httpConfig.enableStaticServe) {
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
        }

        registerCommands(this.controller, app);

        var server = creds ?
            https.createServer(creds, app) :
            http.createServer(app);

        server.listen(this.port);

        var type = creds ? 'HTTPS' : 'HTTP';
        console.log(type, 'server running on port', this.port);
    }

    var registerCommands = function(controller, app) {
        app.get('/resource/:resource/info', function(req, res) {
            controller.info(req.params.resource, function(err, data) {
                if (err) return res.json(err.code || 500, err.message);
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
                        return res.json(err.code || 500, err.message);
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

