// app.js
// Main entry point for pdal session pooling
//

var
	path = require('path'),
	PDALSession = require('./lib/pdal-session').PDALSession;

process.nextTick(function() {
	var s = new PDALSession({
		processPath: path.join(__dirname, '..', 'pdal-session', 'pdal-session')
	});

	s.create("", function(err, success) {
		console.log('Success status: ' + success);

		console.log('getting points');
		s.getNumPoints(function(err, count) {
			console.log('Point count: ', count);
		});
	});
});
