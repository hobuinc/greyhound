process.title = 'gh_cn';

process.on('uncaughtException', function(err) {
    console.error('Caught at top level: ' + err);
});

var console = require('clim')(),
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
});

