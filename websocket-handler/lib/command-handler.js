// command-handler.js
// Handle commands
//


var
	_ = require('lodash');


(function() {
	"use strict";

	var CommandHandler = function(ws) {
		this.ws = ws;
		this.handlers = {};

		var send = function(obj) {
			try {
				ws.send(JSON.stringify(obj));
			}
			catch(e) {
				console.log('Failed to send object: ', obj, e); 
			}
		}

		var o = this;
		this.ws.on('message', function(data) {
			var msg = null;
			try {
				msg = JSON.parse(data);
			} catch(e) {
				return send({
					status: 0,
					reason: 'Couldn\'t parse command'
				});
			}

			if (!msg.command)
				return send({
					status: 0,
					reason: 'Unknown command'
				});

			if (_.isFunction(o.handlers[msg.command])) {
				o.handlers[msg.command](msg, function(err, res) {
					if (err) return send({
						command: msg.command,
						status: 0, reason: err.message });

					send(_.extend(res || {}, { command: msg.command, status: 1 }));
				});
			}
		});
	};

	CommandHandler.prototype.on = function(message, f) {
		this.handlers[message] = f;
	}

	module.exports.CommandHandler = CommandHandler;
})();
