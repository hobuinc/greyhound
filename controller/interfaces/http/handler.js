var
    _ = require('lodash'),
    fs = require('fs'),
    http = require('http'),
	path = require('path'),
    express = require('express'),
    bodyParser = require('body-parser'),
    methodOverride = require('method-override'),
    lessMiddleware = require('less-middleware'),

    console = require('clim')()

    controllerConfig = (require('../../../config').cn || { }),
    httpConfig = (controllerConfig ? controllerConfig.http : { }),
    exposedHeaders =
            'X-Greyhound-Num-Points,' +
            'X-Greyhound-Read-ID,' +
            'X-Greyhound-Raster-Meta'
    ;

if (
        httpConfig.headers &&
        httpConfig.headers['Access-Control-Expose-Headers'])
{
    exposedHeaders +=
        ',' +
        httpConfig.headers['Access-Control-Expose-Headers'];
    delete httpConfig.headers['Access-Control-Expose-Headers'];
}

(function() {
    'use strict';

    var HttpHandler = function(controller, port) {
        this.controller = controller;
        this.port = port;
    }

    HttpHandler.prototype.start = function() {
        var self = this;
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

        // development only
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

        registerCommands(self.controller, app);

        var server = http.createServer(app);
        server.listen(self.port);

        console.log('HTTP server running on port: ' + self.port);
    }

    var extend = function(err, response, command) {
        var common = {
            status: !err,
            command: command
        }

        if (err) common['reason'] = err.message;

        return _.extend(response || { }, common);
    }

    var registerCommands = function(controller, app) {
        app.get('/resource/:resource/numPoints', function(req, res) {
            controller.numPoints(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'numPoints'));
            });
        });

        app.get('/resource/:resource/schema', function(req, res) {
            controller.schema(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'schema'));
            });
        });

        app.get('/resource/:resource/stats', function(req, res) {
            controller.stats(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'stats'));
            });
        });

        app.get('/resource/:resource/srs', function(req, res) {
            controller.srs(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'srs'));
            });
        });

        app.get('/resource/:resource/bounds', function(req, res) {
            controller.bounds(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'bounds'));
            });
        });

        app.get('/resource/:resource/fills', function(req, res) {
            controller.fills(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'fills'));
            });
        });

        app.post('/resource/:resource/serialize', function(req, res) {
            controller.serialize(req.params.resource, function(err, data) {
                res.json(extend(err, data, 'serialize'));
            });
        });

        app.delete('/resource/:resource/readId/:readId', function(req, res) {
            controller.cancel(
                req.params.resource,
                req.params.readId,
                function(err, data) {
                    res.json(extend(err, data, 'cancel'));
                }
            );
        });

        app.get('/resource/:resource/read', function(req, res) {
            var params = req.query;

            controller.read(
                req.params.resource,
                params,
                function(err, props) {
                    if (err) return res.json(err.code || 500, err.message);

                    res.header('X-Greyhound-Num-Points', props.numPoints);
                    res.header('X-Greyhound-Read-ID', props.readId);
                    res.header('Content-Type', 'application/octet-stream');
                    if (props.rasterMeta) {
                        res.header(
                            'X-Greyhound-Raster-Meta',
                            JSON.stringify(shRes.rasterMeta));
                    }
                },
                function(err, data, done) {
                    if (err) return res.json(err.code || 500, err.message);

                    res.write(data);

                    if (done) res.end();
                }
            );
        });
    }

    module.exports.HttpHandler = HttpHandler;
})();

