var net = require('net');

(function() {
    'use strict';

    var Listener = function(onData, onEnd) {
        this.server = null
        this.socket = null;

        this.onData = onData;
        this.onEnd = onEnd;

        this.close = function() {
            var self = this;
            if (self.server) {
                try {
                    self.server.close();
                }
                catch(e) {
                    console.log('Listener error:', e);
                }
            }
            self.server = null;
        }
    }

    Listener.prototype.listen = function(cb) {
        var self = this;

        var server = net.createServer();
        self.server = server;

        server.on('listening', function() {
            cb({
                host: '127.0.0.1',
                port: self.server.address().port
            });
        });

        server.on('connection', function(socket) {
            self.socket = socket;

            socket.on('data', function(data) {
                self.onData(data);
            });

            socket.on('end', function() {
                self.onEnd();
                self.close();
            });
        });

        server.listen(0);
    }

    Listener.prototype.cancel = function() {
        var self = this;
        if (self.socket) {
            self.socket.destroy();
        }

        self.close();
    }

    module.exports.Listener = Listener;
})();

