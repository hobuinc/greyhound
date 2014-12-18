// A websocket client that serializes a pre-created pipeline ID.
var WebSocket = require('ws'),
    argv = require('minimist')(process.argv.slice(2)),
    pipelineId = argv._ && argv._.length == 1 ?
        argv._[0] :
        '58a6ee2c990ba94db936d56bd42aa703'
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
			command: 'create',
            pipelineId: pipelineId
		});
	});

	var count = 0, chunks = 0, toRead = 0;
	var session = null;

	ws.on('message', function(data, flags) {
        var obj = JSON.parse(data);
        console.log('Got back:', obj);

        if (obj.command === 'create' && obj.status === 1) {
            session = obj.session;
            send({
                command: 'serialize',
                session: session
            });
        }
        else if(obj.command === 'serialize' && obj.status === 1) {
            send({
                command: 'destroy',
                session: session
            });
        }
        else if (obj.command == 'destroy' && obj.status === 1) {
            // Session was destroyed succesfully.
            session = null;
            console.log('Done');
            process.exit(0);
        }
	});
}

process.nextTick(run);

