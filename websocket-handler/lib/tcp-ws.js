// tcp-ws.js
// Tcp to websocket binary streaming


var net = require('net'),
	events = require('events');

(function() {
	"use strict";

	var TcpToWs = function(ws) {
		this.ws = ws;

		// the tcp-ws can start recieving data from the server even before
		// we're ok with start pushing data to our client, the session handler
        // server assumes that if you have been able to specify a host:port,
        // you are ready to receive data and starts pushing data.  On the
        // other hand for our ws client, we always want to provide data in
        // order readSuccess -> data -> readComplete. We, therefore, need to
        // stage data till we are sure that the readSuccess message has been
		// sent down the connection.
		this.canPush = false;
		this.waitData = null;
		this.hasEnded = false;

		this.totalArrived = 0;
		this.totalSent = 0;

		events.EventEmitter.call(this);
	}

	TcpToWs.prototype.wsSend = function(data) {
        var self = this;

        // Note: Increment totalSent BEFORE sending (do not use send()'s
        // optional callback).  Once we enter the send routine it may take
        // time to complete, but we can't unsend that data so count it
        // immediately.  This is important for supplying the user an accurate
        // count in the case of a cancel.
        self.totalSent += data.length;
		this.ws.send(data, { binary: true });
	}

	TcpToWs.prototype.start = function() {
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
            o.socket = socket;
			socket.on('end', function() {
				// if the data has finished arriving even before
				// we actually got a chance to push any data at all, we won't
				// notify 'end' till we actually get done pushing data (in
                // the startPushing method)

				if (!o.canPush)
					o.hasEnded = true;
				else {
					o.emit('end');
				}

				safeClose();
			});

			socket.on('data', function(data) {
				if (o.canPush) {
					try {
						o.wsSend(data);
					}
					catch(e) {
						console.log('Failed to push binary blob(push: on)', e);
					}
				}
				else {
					o.waitData = (o.waitData === null) ?
                        data :
                        Buffer.concat([o.waitData, data]);
				}

				o.totalArrived += data.length;
			});

			socket.on('error', function(err) {
				o.emit('error', err);
				safeClose();
			});
		});

		server.listen(0);
		this.server = server;
	};

	TcpToWs.prototype.startPushing = function() {
		if (this.waitData) {
			try {
				this.wsSend(this.waitData);
			}
			catch(e) {
				console.log('Failed to send binary blob', e);
			}
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

    TcpToWs.prototype.cancel = function() {
        // Due to propagation delays between Greyhound components, we will
        // still receive some buffered data after the cancel call.  Destroy
        // socket immediately to halt any further I/O since the read has
        // been canceled.
        //
        // This function should not be called on a clean exit, as it may
        // cause truncated data.
        this.socket.destroy();
        this.close();
    };

	TcpToWs.prototype.close = function() {
		if (this.server)
			try { this.server.close(); } catch(e) { console.log('err', e); }
		this.server = null;
	};

	TcpToWs.prototype.__proto__ = events.EventEmitter.prototype;

	module.exports.TcpToWs = TcpToWs;
})();

