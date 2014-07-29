// pdal-pool.js
// A pool of PdalSession processes
//

var
	_ = require('lodash'),
    PdalSession = require('../build/Release/pdalSession').PdalSession,
	poolModule = require('generic-pool');

(function() {
	"use strict";

	var createProcessPool = function() {
		var pool = poolModule.Pool({
			name: 'pdal-pool',
			create: function(cb) {
                var s = new PdalSession();
				cb(s);
			},

			destroy: function(s) {
                // TODO What if a detached BufferTransmitter is running?
				s.destroy();
			},

			max: 100,
			min: 0,
			idleTimeoutMillis: 10000,
			log: false
		});

		return pool;
	}

	module.exports.createProcessPool = createProcessPool;
})();

