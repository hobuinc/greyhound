// app.js
// Main entry point for pdal session pooling
//

var express = require("express"),
	Q = require('q'),
	path = require('path'),
	crypto = require('crypto'),
	_ = require('lodash'),
	redis = require('redis'),

	createProcessPool = require('./lib/pdal-pool').createProcessPool,

    app     = express(),
    port    = process.env.PORT || 3000,
	pool = null;


// configure express application
app.configure(function(){
  app.use(express.methodOverride());
  app.use(express.bodyParser());
  app.use(express.errorHandler({
    dumpExceptions: true, 
    showStack: true
  }));
  app.use(app.router);
});

var sessions = {};

var getSession = function(res, sessionId, cb) {
	if (!sessions[sessionId])
		return res.json(404, { message: 'No such session' });
	
	cb(sessions[sessionId], sessionId);
};

var error = function(res) {
	return function(err) {
		res.json(500, { message: err.message || 'Unknown error' });
	};
};

var createId = function() {
	return crypto.randomBytes(20).toString('hex');
}

app.get("/", function(req, res) {
	res.json(404, { message: 'Invalid service URL' });
});

// handlers for our API
app.post("/create", function(req, res) {
	pool.acquire(function(err, s) {
		if (err)
			return res.json(500, { message: err.message });

		s.create(req.body.desc || "").then(function() {
			var id = createId();
			sessions[id] = s;

			res.json({ sessionId: id });
		}, function(err) {
			pool.release(s);

			return error(err);
		});
	});
});

app.delete("/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s, sid) {
		delete sessions[sid];


		s.destroy().then(function() {
			pool.release(s);

			res.json({ message: 'Session destroyed' });
		}, function(err) {
			pool.release(s);

			console.log('Destroy was not clean!');
			res.json({ message: 'Session destroyed, but not clean' });
		});
	});
});

app.get("/pointsCount/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s) {
		s.getNumPoints().then(function(count) {
			res.json({ count: count });
		}, error(res));
	});
});

app.post("/read/:sessionId", function(req, res) {
	var host = req.body.host, port = parseInt(req.body.port);

	var start = parseInt(req.body.start);
	var count = parseInt(req.body.count);

	if (!host)
		return res.json(400, { message: 'Destination host needs to be specified' });

	if (!port)
		return res.json(400, { message: 'Destination port needs to be specified' });

	getSession(res, req.params.sessionId, function(s, sid) {
		s.read(host, port, start, count).then(function(r) {
			var ret = _.extend(_.omit(r, 'status'), {
				message: 'Request queued for transmission to: ' + host + ':' + port,
			});
			res.json(ret);
		}, error(res));
	});
});

app.listen(port, function() {
	pool = createProcessPool({
		processPath: path.join(__dirname, '..', 'pdal-session', 'pdal-session'),
		log: false
	});

	// register ourselves with the redis server
	client = redis.createClient();
	client.on('error', function(err) {
		console.log('Redis error: ' + err);
	});

	client.rpush('rh', 'localhost:' + port);

	console.log('Request handler listening on port: ' + port);
				  
});

var cleanup = function() {
	// unregister ourselves with the redis server
	client = redis.createClient();
	client.on('error', function(err) {
		console.log('Redis error: ' + err);
		process.exit(0);
	});

	client.lrem('rh', 0, 'localhost:' + port, function() {
		process.exit(0);
	});
}

process.on('exit', cleanup);
process.on('SIGINT', cleanup);
process.on('SIGHUP', cleanup);
