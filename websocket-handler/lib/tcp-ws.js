// tcp-ws.lib
// Tcp to websocket binary streaming
//


var net = require('net'),
	events = require('events');

(function() {
	"use strict";

	var TCPToWS = function(ws) {
		this.ws = ws;

		// the tcp-ws can start recieving data from the server even before
		// we're ok with start pushing data to our client, the request handler server 
		// assumes that if you have been able to specify a host:port, you are ready to 
		// receive data and starts pushing data.  On the other hand for our ws client, we always
		// want to provide data in order readSuccess -> data -> readComplete. We, therefore,
		// need to stage data till we are sure that the readSuccess message has been
		// sent down the connection.
		this.canPush = false;
		this.waitData = '';
		this.hasEnded = false;

		events.EventEmitter.call(this);
	}

	TCPToWS.prototype.start = function() {
		var server = net.createServer();
		var safeClose = function() {
			try { server.close() } catch(e) {};
		};

		var o = this;
		server.on('listening', function() {
			o.emit('local-address', {
				host: '127.0.0.1',
				port: server.address().port
			});
		});

		server.on('close', function() {
			o.emit('close');
		});

		server.on('error', function(err) {
			o.emit('error', err);
		});

		server.on('connection', function(socket) {
			socket.on('end', function() {
				// if the data has finished arriving even before
				// we actually got a chance to push any data at all, we won't
				// notify 'end' till we actually get done pushing data (in the start
				// pushing method)

				if (!o.canPush)
					o.hasEnded = true;
				else
					o.emit('end');

				safeClose();
			});

			socket.on('data', function(data) {
				if (o.canPush) {
					o.ws.send(data, { binary: true });
				}
				else
					o.waitData = o.waitData + data;
			});

			socket.on('error', function(err) {
				o.emit('error', err);
				safeClose();
			});
		});

		server.listen(0);
		this.server = server;
	};

	TCPToWS.prototype.startPushing = function() {
		if (this.waitData) {
			this.ws.send(this.waitData, { binary: true });
			this.waitData = null;
		}

		this.canPush = true;

		// by the time we got a chance to start pushing data
		// all data has already arrived, make sure we push end
		// once the data is all gone
		if (this.hasEnded) {
			var o = this;
			process.nextTick(function() {
				o.emit('end');
			});
		}
	}

	TCPToWS.prototype.close = function() {
		if (this.server)
			try { this.server.close(); } catch(e) { }
		this.server = null;
	};

	TCPToWS.prototype.__proto__ = events.EventEmitter.prototype;

	module.exports.TCPToWS = TCPToWS;
})();
