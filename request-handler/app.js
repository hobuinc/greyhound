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

    app = express(),
    ports = seaport.connect('localhost', 9090),
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
        console.log('HERE DUDE');
        if (err) {
            console.log('erroring acquire:', s);
            return error(res)(err);
        }

        s.create(req.body.pipeline, function(err) {
            if (err) {
                console.log('erroring create:', err);
                pool.release(s);
                return error(res)(err);
            }

            var id = createId();
            sessions[id] = s;

            console.log('create =', id);
            res.json({ sessionId: id });
        });
    });
});

app.delete("/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('delete('+ sid + ')');
        delete sessions[sid];

        // TODO - Which?
        //pool.destroy(s);
        pool.release(s);
        res.json({ message: 'Session destroyed' });
    });
});

app.get("/pointsCount/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('pointsCount('+ sid + ')');
        res.json({ count: s.getNumPoints() });
    });
});

app.get("/schema/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('schema('+ sid + ')');
        res.json({ schema: s.getSchema() });
    });
});

app.get("/srs/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('srs('+ sid + ')');
        res.json({ srs: s.getSrs() });
    });
});

app.post("/read/:sessionId", function(req, res) {
    var host = req.body.host;
    var port = parseInt(req.body.port);

    if (!host)
        return res.json(
            400,
            { message: 'Destination host needs to be specified' });

    if (!port)
        return res.json(
            400,
            { message: 'Destination port needs to be specified' });

    var start = req.body.hasOwnProperty(start) ? parseInt(req.body.start) : 0;
    var count = req.body.hasOwnProperty(count) ? parseInt(req.body.count) : 0;

    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('read('+ sid + ')');

        s.read(host, port, start, count, function(err, numPoints, numBytes) {
            res.json({
                numPoints: numPoints,
                numBytes: numBytes,
                message:
                    'Request queued for transmission to ' + host + ':' + port,
            });
        });
    });
});

var port = ports.register('rh@0.0.1');
app.listen(port, function() {
    pool = createProcessPool();

    console.log('Request handler listening on port: ' + port);
});

