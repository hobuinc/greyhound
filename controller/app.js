process.title = 'gh_cn';

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
    if (config.hasOwnProperty('ws') && config.ws.enable) {
        var wsHandler = new WsHandler(controller, config.ws.port);
        wsHandler.start();
    }

    if (config.hasOwnProperty('http') && config.http.enable) {
        var httpHandler = new HttpHandler(controller, config.http.port);
        httpHandler.start();
    }

    if (config.hasOwnProperty('https') && config.https.enable) {
        if (!config.https.keyFile || !config.https.certFile) {
            throw new Error('HTTPS enabled, but no certFile/keyFile specified');
        }

        var httpsHandler = new HttpHandler(controller, config.https.port);

        var getPath = (file) => path.join(__dirname, '../', file);
        var key = fs.readFileSync(getPath(config.https.keyFile));
        var cert = fs.readFileSync(getPath(config.https.certFile));

        httpsHandler.start({ key: key, cert: cert });
    }
});

