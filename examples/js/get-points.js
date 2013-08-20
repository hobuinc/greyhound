// get-points.js
// A websocket clients that queries points from our service
//

var WebSocket = require('ws');


var run = function() {
	var ws = new WebSocket('ws://localhost:' + (process.env.PORT || 80) + '/');

	// helper method to send json objects
	//
	var send = function(obj) {
		ws.send(JSON.stringify(obj));
	}

	ws.on('open', function() {
		// as soon as the socket opens, send command to create
		// a session
		send({
			command: 'create'
		});
	});

	var count = 0, chunks = 0, toRead = 0;
	var session = null;
	ws.on('message', function(data, flags) {
		// if the data is binary its most likey from one of
		// our calls to get points, update current state
		//
		if (flags.binary) {
			count += data.length;
			chunks++;
			if (count >= toRead) {
				console.log('Read', count, 'bytes in', chunks, 'chunks');
				// done reading all bytes, now destroy session
				//
				send({
					command: 'destroy',
					session: session
				});
			}
		}
		else {
			// non binary, parse it in and figure what it is
			var obj = JSON.parse(data);
			if (obj.command === 'create' && obj.status === 1) {
				// if the create command succeeded, send a request to read everything
				// 
				session = obj.session;
				send({
					command: 'read',
					session: session
				});
			}
			else if(obj.command === 'read' && obj.status === 1) {
				// Read command was successful, we should not wait for binary data to arrive
				//
				console.log('Total bytes to read: ' + obj.bytesCount);
				count = chunks = 0;
				toRead = obj.bytesCount;
			}
			else if (obj.command == 'destroy' && obj.status === 1) {
				// session was destroyed
				session = null;
				console.log('Done');
				process.exit(0);
			}
		}
	});
}

process.nextTick(run);
