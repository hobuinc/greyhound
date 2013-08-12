// test.js
// A websocket test client
//

var WebSocket = require('ws');

var ws = new WebSocket('ws://localhost:' + (process.env.PORT || 3000) + '/');
var send = function(obj) {
	ws.send(JSON.stringify(obj));
}

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
		send({
			command: 'pointsCount'
		});

		setTimeout(function() {
			send({
				command: 'destroy'
			});
		}, 1000);
	}
});
