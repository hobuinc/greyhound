// pdal-session.js
// Abstracts a PDAL process
//

var
	_ = require('lodash'),
	Parser = require('jsonparse')

	spawn = require('child_process').spawn,
	fs = require('fs'),
	path = require('path');

(function() {
	"use strict";

	var PDALSession = function(options) {
		this.options = _.defaults({
			log: true
		}, options);

		if (!this.options.processPath)
			throw new Error('You need to specify the process path for pdal-session executable');

		if (!this.options.workingDirectory)
			this.options.workingDirectory = path.dirname(this.options.processPath);

		console.log(this.options);

		this.ps = null;
	};

	PDALSession.prototype.checkInvoke = function() {
		var o = this;
		if (o.reqCB) {
			var f = o.reqCB;
			o.reqCB = null;
			f.apply(o, arguments);
		}
	}

	PDALSession.prototype.checkSpawn = function(cb) {
		var o = this;

		if (o.ps)
			return cb(null, o.ps);

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
			if (o.options.log)
				console.log('Session closed with code: ', code);

			o.checkInvoke(new Error('Session closed while waiting for response'));
			o.ps = null;
		});

		ps.on('exit', function(code, sig) {
			if (o.options.log)
				console.log('Contained process exiting: ', code, sig);

			o.checkInvoke(new Error('Session closed while waiting for response'));
			o.ps = null;
		});

		// we have to wait for the process to become active,
		// it will notify us with a ready message
		o.reqCB = function(err, res) {
			if (err || res.ready !== 1) return cb(err || new Error('Not ready'));

			o.ps = ps;
			cb(null, o.ps);
		};
	};

	PDALSession.prototype.exchange = function(obj, cb) {
		var o = this;

		o.checkSpawn(function(err, ps) {
			if (err) return cb(err);

			o.reqCB = cb;
			var buf = JSON.stringify(obj) + '\n';
			if (o.options.log) {
				console.log('stdin >>>>>>>>>>>>>>');
				console.log(buf)
			}

			o.ps.stdin.write(buf);
		});
	}

	PDALSession.prototype.create = function(desc, cb) {
		this.exchange({
			command: 'create',
			params: {
				pipelineDesc: desc
			}
		}, function(err, res) {
			if (err || res['status'] != 1)
				return cb(null, false);
			return cb(null, true);
		});
	}

	PDALSession.prototype.destroy = function(cb) {
		this.exchange({
			command: 'destroy',
		}, function(err, res) {
			if (err || res['status'] != 1)
				return cb(null, false);
			return cb(null, true);
		});
	}

	PDALSession.prototype.getNumPoints = function(cb) {
		this.exchange({
			command: 'getNumPoints',
		}, function(err, res) {
			if (err || res.status !== 1) 
				return cb(err || new Error('Non-successful response'));
			cb(null, res.count);
		})
	}

	PDALSession.prototype.isValid = function(cb) {
		this.exchange({
			command: 'isValid',
		}, function(err, res) {
			if (err || res.status != 1)
				return cb(err || new Error('Non-successful response'));
			cb(null, res.valid);
		});
	}

	module.exports.PDALSession = PDALSession;
})();
