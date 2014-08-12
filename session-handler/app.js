// app.js
// Main entry point for pdal session pooling
//
process.title = 'gh_sh';

var express = require("express"),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser'),
    Q = require('q'),
    path = require('path'),
    crypto = require('crypto'),
    _ = require('lodash'),
    seaport = require('seaport'),

    PdalSession = require('./build/Release/pdalBindings').PdalBindings,
	poolModule = require('generic-pool'),

    app = express(),
    ports = seaport.connect('localhost', 9090),
    pool = null;

// configure express application
app.configure(function() {
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
app.get("/validate", function(req, res) {
    pool.acquire(function(err, s) {
        if (err) {
            console.log('erroring acquire on validation:', s);
            return error(res)(err);
        }

        s.parse(req.body.pipeline, function(err) {
            pool.destroy(s);
            if (err) console.log('Pipeline validation error:', err);
            res.json({ valid: err ? false : true });
        });
    });
});

app.post("/create", function(req, res) {
    pool.acquire(function(err, s) {
        if (err) {
            console.log('erroring acquire:', s);
            return error(res)(err);
        }

        s.create(req.body.pipeline, function(err) {
            if (err) {
                console.log('erroring create:', err);
                pool.destroy(s);
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

        pool.destroy(s);
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

app.post("/cancel/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('Got CANCEL request');
        res.json({ 'cancelled': s.cancel() });
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

    getSession(res, req.params.sessionId, function(s, sid) {
        console.log('read('+ sid + ')');

        var readHandler = function(err, numPoints, numBytes) {
            if (err) {
                console.log('Erroring read:', err);
                return res.json(400, { message: err });
            }
            else {
                return res.json({
                    numPoints: numPoints,
                    numBytes: numBytes,
                    message:
                        'Request queued for transmission to ' +
                        host + ':' + port,
                });
            }
        };

        if (
            req.body.hasOwnProperty('start') ||
            req.body.hasOwnProperty('count') ||
            Object.keys(req.body).length == 2) {

            // Unindexed read - 'start' and 'count' may be omitted.  If either
            // of them exists, or if the only arguments are host+port, then
            // use this branch.

            var start = req.body.hasOwnProperty('start') ?
                parseInt(req.body.start) : 0;
            var count = req.body.hasOwnProperty('count') ?
                parseInt(req.body.count) : 0;

            if (start < 0) start = 0;
            if (count < 0) count = 0;

            s.read(host, port, start, count, readHandler);
        }
        else if (
            req.body.hasOwnProperty('radius') &&
            req.body.hasOwnProperty('x') &&
            req.body.hasOwnProperty('y')) {

            // Indexed read: point + radius query.

            var is3d = req.body.hasOwnProperty('z');
            var radius = parseFloat(req.body.radius);
            var x = parseFloat(req.body.x);
            var y = parseFloat(req.body.y);
            var z = is3d ? z : 0.0;

            s.read(host, port, is3d, radius, x, y, z, readHandler);
        }
        else {
            return res.json(
                400,
                { message: 'Invalid read command - bad params' });
        }
    });
});

var port = ports.register('sh@0.0.1');
app.listen(port, function() {
    pool = poolModule.Pool({
        name: 'pdal-pool',
        create: function(cb) {
            var s = new PdalSession();
            cb(s);
        },

        destroy: function(s) {
            s.destroy();
        },

        max: 100,
        min: 0,
        idleTimeoutMillis: 10000,
        log: false
    });

    console.log('Session handler listening on port: ' + port);
});

