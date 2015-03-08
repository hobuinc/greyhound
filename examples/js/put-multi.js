#!/usr/bin/env node

var WebSocket = require('ws');
//var manifest = require('./manifest');
var manifest = require('./broken');
console.log('Sending manifest paths:', manifest.length);

var utm = [
    192325.727246, 4455899.672669,
    770274.931198, 4833815.152277
];
var wbm = [
    -10796577.371225, 4902908.135781,
    -10015953.953824, 5375808.896799
];

var run = function() {
    //var id = 'local';
    //var id = 'first';
    var id = 'bigger';


    var bbox;
    var ws;

    if (id == 'local') {
        ws = new WebSocket('ws://localhost:8080/');
        //bbox = wbm;
    }
    else if (id == 'first') {
        ws = new WebSocket('ws://52.0.151.178:8989/');
        //bbox = utm;
    }
    else if (id == 'bigger') {
        ws = new WebSocket('ws://54.198.162.84:8989/');
        //bbox = utm;
    }

    bbox = utm;

    // Helper method to send json objects.
    var send = function(obj) {
        var s = JSON.stringify(obj);
        console.log('Sending');
        ws.send(s);
    }

    console.log('bbox: ' + bbox);

    ws.on('open', function() {
        // As soon as the socket opens, send command to create a session.
        send({
            command: 'put',
            path: manifest,
            bbox: bbox
        });
    });

    var count = 0, chunks = 0, toRead = 0;
    var session = null;

    ws.on('message', function(data, flags) {
        var obj = JSON.parse(data);
        console.log('Got back:', obj);

        if(obj.command === 'put' && obj.status === 1) {
            console.log('pipelineId:', obj.pipelineId);
            process.exit(0);
        }
        else {
            console.log('ERROR:', JSON.stringify(obj, null, 3));
            process.exit(1);
        }
    });
}

process.nextTick(run);

