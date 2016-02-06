process.title = 'greyhound';

process.on('uncaughtException', function(err) {
    console.error('Caught at top level: ' + err);
});

var console = require('clim')(),
    fs = require('fs'),
    path = require('path'),
    config = (require('../config').cn || { }),

    Controller = require('./controller').Controller,
    WsHandler = require('./interfaces/ws/handler').WsHandler,
    HttpHandler = require('./interfaces/http/handler').HttpHandler
    ;

var controller = new Controller();

process.nextTick(function() {
    if (config.ws && config.ws.port) {
        var wsHandler = new WsHandler(controller, config.ws.port);
        wsHandler.start();
    }

    if (config.http && (config.http.port || config.http.securePort)) {
        var http = config.http;
        var creds = null;

        if (http.keyFile && http.certFile && http.securePort) {
            var getPath = (file) => {
                if (file == 'key.pem' || file == 'cert.pem') {
                    return path.join(__dirname, '../', file);
                }
                else return path;
            };

            var key = fs.readFileSync(getPath(http.keyFile));
            var cert = fs.readFileSync(getPath(http.certFile));

            creds = { key: key, cert: cert };
        }

        var httpHandler = new HttpHandler(
            controller,
            http.port,
            http.securePort,
            creds);

        httpHandler.start();
    }
});

