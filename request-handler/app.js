// app.js
// Main entry point for pdal session pooling
//
process.title = 'gh_rh';

var express = require("express"),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser'),
	Q = require('q'),
	path = require('path'),
	crypto = require('crypto'),
	_ = require('lodash'),
	seaport = require('seaport'),

	createProcessPool = require('./lib/pdal-pool').createProcessPool,

    app     = express(),
	ports	= seaport.connect('localhost', 9090),
	pool = null;


// configure express application
app.configure(function(){
  app.use(methodOverride());
  app.use(bodyParser());
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
		if (err) {
            console.log('erroring: ', s);
			return error(res)(err);
        }

		s.create(req.body.pipeline || "").then(function() {
			var id = createId();
			sessions[id] = s;

			console.log('create =', id);
			res.json({ sessionId: id });
		}, function(err) {
            console.log('erroring create: ', err);            
			pool.release(s);
			return error(res)(err);
		}).done();
	});
});

app.delete("/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s, sid) {
		console.log('delete('+ sid + ')');
		delete sessions[sid];

		s.destroy().then(function() {
			pool.release(s);

			res.json({ message: 'Session destroyed' });
		}, function(err) {
			pool.release(s);

			console.log('Destroy was not clean!');
			res.json({ message: 'Session destroyed, but not clean' });
		}).done();
	});
});

app.get("/pointsCount/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s, sid) {
		console.log('pointsCount('+ sid + ')');
		s.getNumPoints().then(function(count) {
			res.json({ count: count });
		}, error(res)).done();
	});
});

app.get("/schema/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s, sid) {
		console.log('schema('+ sid + ')');
		s.getSchema().then(function(schema) {
			res.json({ schema: schema});
		}, error(res)).done();
	});
});

app.get("/srs/:sessionId", function(req, res) {
	getSession(res, req.params.sessionId, function(s, sid) {
		console.log('srs('+ sid + ')');
		s.getSRS().then(function(srs) {
			res.json({ srs: srs });
		}, error(res)).done();
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
		console.log('read('+ sid + ')');

		s.read(host, port, start, count).then(function(r) {
			var ret = _.extend(_.omit(r, 'status'), {
				message: 'Request queued for transmission to: ' + host + ':' + port,
			});
			res.json(ret);
		}, error(res)).done();
	});
});

var port = ports.register('rh@0.0.1');
app.listen(port, function() {
	pool = createProcessPool({
		processPath: path.join(__dirname, '..', 'pdal-session', 'pdal-session'),
		log: false
	});

	console.log('Request handler listening on port: ' + port);
});

