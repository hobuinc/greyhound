// a simple websocket server
//
var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({port: 8080});

wss.on('connection', function(ws) {
    ws.on('message', function(message) {
		ws.send('Sending back what you sent me: ' + message);
    });

	ws.send('Welcome');
});
