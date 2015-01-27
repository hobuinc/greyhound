// A websocket client that queries points from Greyhound.
var WebSocket = require('ws');

var run = function() {
	var ws = new WebSocket('ws://localhost:8080/');

	// Helper method to send json objects.
	var send = function(obj) {
		ws.send(JSON.stringify(obj));
	}

	ws.on('open', function() {
		send({
			command: 'read',
            pipeline: '5adcf597e3376f98471bf37816e9af2c'
		});
	});

	var count = 0, chunks = 0, toRead = 0;
	var session = null;

	ws.on('message', function(data, flags) {
		// Greyhound's only binary output is a 'read' command result.
		if (flags.binary) {
            // Update our received counts.
			count += data.length;
			chunks++;

			if (count >= toRead) {
				console.log('Read', count, 'bytes in', chunks, 'chunks');
				// Done reading all bytes - destroy the session.
                console.log('Done');
                process.exit(0);
			}
		}
		else {
			// Data is non-binary, parse and identify it.
			var obj = JSON.parse(data);
			if(obj.command === 'read' && obj.status === 1) {
				// Initial info for the details of the ensuing binary data.
				console.log('Total bytes to read: ' + obj.numBytes);
				count = chunks = 0;
				toRead = obj.numBytes;
			}
		}
	});
}

process.nextTick(run);

