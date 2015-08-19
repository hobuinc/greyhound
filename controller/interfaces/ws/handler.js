var
    WebSocketServer = require('ws').Server,
    http = require('http'),
    express = require('express'),
    console = require('clim')(),

    Commander = require('./commander').Commander,

    numBytes = { }
    ;

(function() {
    'use strict';

    var WsHandler = function(controller, port) {
        this.controller = controller;
        this.port = port;
    }

    WsHandler.prototype.start = function() {
        var self = this;

        var app = express();
        app.use(function(req, res, next) {
            res.header('X-Powered-By', 'Hobu, Inc.');
            next();
        });

        app.get('/', function(req, res) {
            res.send('Hobu, Inc. point distribution server');
        });

        var server = http.createServer(app);

        server.listen(self.port);

        var wss = new WebSocketServer({ server: server });

        console.log('Websocket server running on port: ' + self.port);

        wss.on('connection', function(ws) {
            console.log("websocket::connection");
            var commander = new Commander(ws);
            registerCommands(self.controller, commander, ws);
        });
    }

    var registerCommands = function(controller, commander, ws) {
        commander.on('info', function(msg, cb) {
            controller.info(msg.resource, cb);
        });

        commander.on('read', function(msg, cb) {
            var params = msg;

            var pipeline = params.resource;
            var summary = params.summary;

            delete params.resource;
            delete params.summary;

            var readId;

            controller.read(
                pipeline,
                params,
                function(err, res) {
                    if (!err) readId = res.readId;
                    numBytes[readId] = 0;
                    cb(err, res);
                },
                function(err, data, done) {
                    if (err) console.log('TODO - handle data error in READ');

                    numBytes[readId] += data.length;
                    ws.send(data, { binary: true });

                    if (done && summary) {
                        ws.send(
                            JSON.stringify({
                                'command':  'summary',
                                'status':   1,
                                'readId':   readId,
                                'numBytes': numBytes[readId]
                            }),
                            null,
                            function() {
                                delete numBytes[readId];
                            }
                        );
                    }
                }
            );
        });
    }

    module.exports.WsHandler = WsHandler;
})();

