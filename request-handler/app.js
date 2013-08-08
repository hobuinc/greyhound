// app.js
// Main entry point for pdal session pooling
//

var
	path = require('path'),
	PDALSession = require('./lib/pdal-session').PDALSession;

process.nextTick(function() {
	var s = new PDALSession({
		processPath: path.join(__dirname, '..', 'pdal-session', 'pdal-session'),
		log: false
	});

	s.create("").then(function() {
		return s.isValid()
	}).then(function(valid) {
		if (valid)
			return s.getNumPoints();
		else
			throw new Error("The session is not valid");
	}).then(function(count) {
		console.log('Total points in request: ' + count);
	}).fail(function(err) {
		console.log('Error', err);
	});
});
