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

		var o = this;
		this.ws.on('message', function(data) {
			var msg = null;
			try {
				msg = JSON.parse(data);
			} catch(e) {
				return o.send({
					status: 0,
					reason: 'Couldn\'t parse command'
				});
			}

			if (!msg.command)
				return o.send({
					status: 0,
					reason: 'Unknown command'
				});

			if (_.isFunction(o.handlers[msg.command])) {
				o.handlers[msg.command](msg, function(err, res) {
					if (err) {
                        return o.send({
                            command: msg.command,
                            status: 0,
                            reason: err.message,
                        });
                    }
                    else {
                        return o.send(_.extend(
                            res || { },
                            { command: msg.command, status: 1 }
                       ));
                    }
				});
			}
            else {
                console.log('Improper configuration');
                return o.send({
                    status: 0,
                });
            }
		});
	};

    CommandHandler.prototype.send = function(obj) {
        try {
            this.ws.send(JSON.stringify(obj));
        }
        catch(e) {
            console.log('Failed to send object: ', obj, e);
        }
    }

	CommandHandler.prototype.on = function(message, f) {
		this.handlers[message] = f;
	}

	module.exports.CommandHandler = CommandHandler;
})();

