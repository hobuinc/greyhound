// a simple websocket server
//
var WebSocketServer = require('ws').Server
  , _ = require('lodash')
  , http = require('http')
  , qs = require('querystring')
  , wss = new WebSocketServer({port: 8080});

var reqHandler = {
	host: 'localhost',
	port: 3000
};

var request = function(method, path, data, cb) {
	var d = data ? qs.stringify(data) : '';

	var options = {
		host: reqHandler.host,
		port: reqHandler.port,
		path: path,
		method: method,
		headers: {
			'Content-Type': 'application/x-www-form-urlencoded',
			'Content-Length': d.length
		}
	};

	var r = http.request(options, function(res) {
		if (res.statusCode / 100 !== 2)
			return cb(new Error('Unsuccessful error code: ' + res.statusCode));

		if (res.headers['content-type'].indexOf('application/json') !== 0)
			return cb(new Error('application/json response type is expected'));

		var contentLength = parseInt(res.headers['content-length']);
		if (!contentLength)
			return cb(new Error('Respond had invalid content-length field'));

		res.setEncoding('utf8');

		var d = ''
		res.on('data', function(c) {
			d = d + c;
			if (d.length >= contentLength)
				cb(JSON.parse(d));
		});
	});

	res.on('error', cb);

	req.end();
};

var post = function(path, data, cb) {
	if (_.isFunction(data)) {
		cb = data;
		data = null;
	}

	request('POST', path, data, cb)
}

var get = function(path, data, cb) {
	if (_.isFunction(data)) {
		cb = data;
		data = null;
	}

	request('GET', path, data, cb)
}

var send = function(ws, obj) {
	ws.send(JSON.stringify(obj), function() { });
}

wss.on('connection', function(ws) {
    ws.on('message', function(message) {
		var session;
		var msg = JSON.parse(message);

		if (msg.command === 'create') {
			if (session)
				return send({ command: 'create', status: 0, reason: 'A session has already been created' })

			post('/create', function(err, res) {
				if (err) 
					return send({ command: 'create', status: 0, reason: err.message })

				session = res.sessionId;

				send({ command: 'create', status: 1 });
			});
		}
		else if (msg.command === 'destroy') {
			if (session)
				return send({ command: 'destroy', status: 0, reason: 'A session has not been created' })

			post('/destroy/' + session, function(err, res) {
				if (err) 
					return send({ command: 'destroy', status: 0, reason: err.message })

				session = ''
				send({ command: 'destroy', status: 1 });
			});
		}
    });
});
