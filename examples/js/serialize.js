#!/usr/bin/env node

// A websocket client that serializes a pre-created pipeline ID.
var WebSocket = require('ws'),
    argv = require('minimist')(process.argv.slice(2)),
    pipelineId = argv._ && argv._.length == 1 ?
        argv._[0] :
        '5adcf597e3376f98471bf37816e9af2c'
    ;

var run = function() {
    var ws = new WebSocket('ws://localhost:8080/');

    // Helper method to send json objects.
    var send = function(obj) {
        ws.send(JSON.stringify(obj));
    }

    ws.on('open', function() {
        // As soon as the socket opens, send command to create a session.
        send({
            command: 'serialize',
            pipeline: pipelineId
        });
    });

    var count = 0, chunks = 0, toRead = 0;
    var session = null;

    ws.on('message', function(data, flags) {
        var obj = JSON.parse(data);
        console.log('Got back:', obj);

        if(obj.command === 'serialize' && obj.status === 1) {
            process.exit(0);
        }
        else {
            console.log('ERROR');
            process.exit(1);
        }
    });
}

process.nextTick(run);

