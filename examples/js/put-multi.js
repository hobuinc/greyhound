#!/usr/bin/env node

// A websocket client that serializes a pre-created pipeline ID.
var WebSocket = require('ws');
var manifest = require('./manifest');

var run = function() {
	var ws = new WebSocket('ws://localhost:8080/');

	// Helper method to send json objects.
	var send = function(obj) {
		ws.send(JSON.stringify(obj));
	}

	ws.on('open', function() {
		// As soon as the socket opens, send command to create a session.
		send({
			command: 'put',
            path: manifest,
            /*
            path: [
                '/vagrant/examples/test/a.laz',
                '/vagrant/examples/test/b.laz',
                '/vagrant/examples/test/c.laz',
                '/vagrant/examples/test/d.laz',
                '/vagrant/examples/test/e.laz',
                '/vagrant/examples/test/f.laz',
                '/vagrant/examples/test/g.laz',
                '/vagrant/examples/test/h.laz',
                //
                '/vagrant/examples/test/i.laz',
                '/vagrant/examples/test/j.laz',
                '/vagrant/examples/test/k.laz',
                '/vagrant/examples/test/l.laz',
                '/vagrant/examples/test/m.laz',
                '/vagrant/examples/test/n.laz',
                '/vagrant/examples/test/o.laz',
                '/vagrant/examples/test/p.laz',
                '/vagrant/examples/test/q.laz',
                '/vagrant/examples/test/r.laz',
                '/vagrant/examples/test/s.laz',
                '/vagrant/examples/test/t.laz',
                //
                '/vagrant/examples/test/u.laz',
                '/vagrant/examples/test/v.laz',
                '/vagrant/examples/test/w.laz',
                '/vagrant/examples/test/x.laz',
                '/vagrant/examples/test/y.laz',
                '/vagrant/examples/test/z.laz',
                '/vagrant/examples/test/za.laz',
                '/vagrant/examples/test/zb.laz',
                '/vagrant/examples/test/zc.laz',
                '/vagrant/examples/test/zd.laz',
                '/vagrant/examples/test/ze.laz',
                '/vagrant/examples/test/zf.laz'
            ],
            */
            // Whole state web mercator:
            bbox: [
                -10796577.371225, 4902908.135781,
                -10015953.953824, 5375808.896799
            ]
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

