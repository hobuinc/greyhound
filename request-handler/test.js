// app.js
// Main entry point for pdal session pooling
//

var
	path = require('path'),
	Q = require('q'), 

	PDALSession = require('./lib/pdal-session').PDALSession;

var waitForTCPData = function(port) {
	var d = Q.defer();
	var bytesRead = 0;

	var net = require('net');
	var server = net.createServer(function(socket) {
		socket.on('end', function() {
			d.resolve(bytesRead);
		});

		socket.on('data', function(data) {
			bytesRead = bytesRead + data.length;
		});

		socket.on('error', function(err) {
			d.reject(err);
		});
	});

	server.listen(port, function() {
		console.log('Data collection server is listening on port: ' + port);
	});

	return d.promise;
};

process.nextTick(function() {
	var s = new PDALSession({
		processPath: path.join(__dirname, '..', 'pdal-session', 'pdal-session'),
		log: false
	});

	console.log('Creating session ...');
	var sessionp = s.create("");
		
	// get and report points
	sessionp.then(function() {
		return s.getNumPoints();
	}).then(function(count) {
		console.log('Total points in request: ' + count);
	}).then(function() {
		var port = 50000 + Math.floor(Math.random() * 10000);
		return s.read('localhost', port).then(function(r) {
			console.log('Total points to read:', r.pointsRead, '(' + r.bytesCount + ' bytes)');
			return waitForTCPData(port);
		});
	}).then(function(count) {
		console.log('Total bytes read: ' + count);
	}).fail(function(err) {
		console.log('Error: ' + err);
	}).then(function() {
		process.exit(0);
	});
});
