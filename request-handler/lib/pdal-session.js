// pdal-session.js
// Abstracts a PDAL process
//

var
	_ = require('lodash'),
	Parser = require('jsonparse')

	Q = require('q'),

	spawn = require('child_process').spawn,
	fs = require('fs'),
	path = require('path');

(function() {
	"use strict";

	var PDALSession = function(options) {
		this.options = _.defaults(options, {
			log: true
		});

		if (!this.options.processPath)
			throw new Error('You need to specify the process path for pdal-session executable');

		if (!this.options.workingDirectory)
			this.options.workingDirectory = path.dirname(this.options.processPath);

		this.ps = null;
	};

	PDALSession.prototype.checkInvoke = function(err, res) {
		var o = this;

		if (o.reqd) {
			var d = o.reqd;
			o.reqd = null;

			console.log('checkInvoke:', err, res);

			if (err)
				return d.reject(err)
			else if (res && (res.status === 0 || res.ready === 0))
				return d.reject(new Error(res.message || "Unsuccessful return"));

			d.resolve(res || {});
		}
	}

	PDALSession.prototype.spawn = function() {
		var o = this;
		var d = Q.defer();

		if (o.ps) {
			d.resolve(o.ps);
			return d.promise;
		}

		var p = new Parser();
		p.onValue = function(value) {
			// TODO: Not sure how to handle objects inside objects yet
			//
			if (!_.isObject(value))
				return;	// only let full objects through

			if (o.options.log)
				console.log('parsed object: ' + JSON.stringify(value));

			o.checkInvoke(null, value);
		}

		var ps = spawn(o.options.processPath, [], {
			cwd: o.options.workingDirectory,
			stdio: 'pipe',
		});

		ps.stdout.on('data', function(data) {
			if (o.options.log) {
				console.log('stdout <<<<<<<<<<<<<<');
				console.log(data.toString());
			}

			p.write(data);
		});

		ps.stderr.on('data', function(data) {
			if (o.options.log) {
				console.log('stderr <<<<<<<<<<<<<<');
				console.log(data.toString());
			}

			o.checkInvoke(new Error('Session responded with error'));
		});

		ps.on('close', function(code) {
            // if (o.options.log)
			console.log('Session closed with code: ', code);

			o.checkInvoke(new Error('Session closed while waiting for response'));
			o.ps = null;
		});

		ps.on('exit', function(code, sig) {
            // if (o.options.log)
			console.log('Contained process exiting: ', code, sig);

			o.checkInvoke(new Error('Session exited while waiting for response'));
			o.ps = null;
		});

		// we have to wait for the process to become active,
		// it will notify us with a ready message
		o.reqd = Q.defer();
		
		return o.reqd.promise.then(function(value) {
			if (value.ready === 1) {
				o.ps = ps;
				return o.ps;
			}
			else
				throw new Error("Couldn't get container process to ready state");
		});
	};

	PDALSession.prototype.exchange = function(obj) {
		var o = this;
        console.log("exchange: ", obj)
		return o.spawn().then(function(ps) {
			var b = JSON.stringify(obj) + '\n';
			if (o.options.log) {
				console.log('stdin >>>>>>>>>>>>>>');
				console.log(b)
			}

			var d = Q.defer();
			o.reqd = d;

			ps.stdin.write(b);
			return d.promise;
		});
	}

	PDALSession.prototype.create = function(desc) {
		return this.exchange({
			command: 'create',
			params: {
                filename :"/vagrant/examples/data/read.xml",
                debug: true,
                verbose: 4
			}
		}).then(function(obj) { 
			// if successful, don't do anything
		});
	}

	PDALSession.prototype.destroy = function() {
		return this.exchange({
			command: 'destroy',
		}).then(function(res) {
		});
	}

	PDALSession.prototype.getNumPoints = function() {
		return this.exchange({
			command: 'getNumPoints',
		}).then(function(res) {
			return res.count;
		});
	};

	PDALSession.prototype.getSRS = function() {
		return this.exchange({
			command: 'getSRS',
		}).then(function(res) {
			return res.srs;
		});
	};

	PDALSession.prototype.isValid = function() {
		return this.exchange({
			command: 'isSessionValid',
		}).then(function(res) {
			return res.valid;
		});
	};

	PDALSession.prototype.read = function(host, port, start, count) {
		var params = {}
		if (start && count) {
			params['start'] = start;
			params['count'] = count;
		}

		return this.exchange({
			command: 'read',
			params: _.extend(params, {
				transmitHost: host,
				transmitPort: port,
			})
		});
	};

	PDALSession.prototype.kill = function() {
		if (this.ps) {
			process.kill(this.ps.pid, 'SIGHUP');
			this.ps = null;
		}
	}

	module.exports.PDALSession = PDALSession;
})();
