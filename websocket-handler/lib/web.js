// web.js
// Web helper functions
//

var
	http = require('http'),
	qs = require('querystring'),
	_ = require('lodash');

(function() {
	"use strict";

	var request = function(reqHandler, method, path, data, cb) {
		var d = data ? qs.stringify(data) : '';

		var parts = reqHandler.split(':');
		if (parts.length !== 2)
			return cb(new Error('The request handler location doesnt look valid'));

		var host = parts[0];
		var port = parseInt(parts[1]);
		if (!port)
			return cb(new Error('Port spec for request handler is not valid'));

		var options = {
			host: host,
			port: port,
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
					cb(null, JSON.parse(d));
			});
		});
		r.on('error', cb);

		r.write(d + '\n');
		r.end();
	};

	var post = function(reqHandler, path, data, cb) {
		if (_.isFunction(data)) {
			cb = data;
			data = null;
		}

		request(reqHandler, 'POST', path, data, cb)
	};

	var _delete = function(reqHandler, path, data, cb) {
		if (_.isFunction(data)) {
			cb = data;
			data = null;
		}

		request(reqHandler, 'DELETE', path, data, cb);
	};

	var get = function(reqHandler, path, data, cb) {
		if (_.isFunction(data)) {
			cb = data;
			data = null;
		}

		request(reqHandler, 'GET', path, data, cb);
	};

	module.exports.get = get;
	module.exports._delete = _delete;
	module.exports.post = post;
})();
