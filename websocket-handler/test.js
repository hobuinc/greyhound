// test.js
// A websocket test client
//

var WebSocket = require('ws');

var ws = new WebSocket('ws://localhost:' + (process.env.PORT || 3000) + '/');
var send = function(obj) {
	ws.send(JSON.stringify(obj));
}

var test1 = function() {
	var session = null;

	ws.on('open', function() {
		setInterval(function() {
			send({
				command: 'create'
			});
		}, 3000);
	});

	ws.on('message', function(data) {
		var obj = JSON.parse(data);

		console.log('Got something: ');
		console.log(obj);

		if (obj.command === 'create' && obj.status === 1) {
			session = obj.session;
			send({
				command: 'pointsCount',
				session: session
			});

			setTimeout(function() {
				send({
					command: 'destroy',
					session: session
				});
			}, 1000);
		}
	});
};

var test2 = function() {
	var start = function() {
		send({
			command: 'create'
		});
	}

	ws.on('open', start);
	var count = 0, chunks = 0, toRead = 0;
	var session = null;

	ws.on('message', function(data, flags) {
		if (flags.binary) {
			count += data.length;
			chunks++;
			if (count >= toRead) {
				console.log('Read', count, 'bytes in', chunks, 'chunks');
				send({
					command: 'destroy',
					session: session
				});
			}
		}
		else {
			var obj = JSON.parse(data);

			console.log('<<<<');
			console.log(obj);

			if (obj.command === 'create' && obj.status === 1) {
				session = obj.session;
				send({
					command: 'read',
					session: session
				});
			}
			else if(obj.command === 'read' && obj.status === 1) {
				console.log('Total bytes to read: ' + obj.bytesCount);
				count = chunks = 0;
				toRead = obj.bytesCount;
			}
			else if (obj.command == 'destroy' && obj.status === 1) {
				session = null;
				setTimeout(start, 1000);
			}
		}
	});
}

process.nextTick(test2);
