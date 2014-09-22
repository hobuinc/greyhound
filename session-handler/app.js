// app.js
// Main entry point for pdal session delegation
//
process.title = 'gh_sh';

var express = require("express"),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser'),
    Q = require('q'),
    path = require('path'),
    crypto = require('crypto'),
    _ = require('lodash'),
    disco = require('../common').disco,

    PdalSession = require('./build/Release/pdalBindings').PdalBindings,

    app = express();

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

var sessions    = { }; // sessionId -> pdalSession (may be many to one)
var pipelineIds = { }; // pipelineId -> pdalSession (one to one)

var getSession = function(res, sessionId, cb) {
    if (!sessions[sessionId])
        return res.json(404, { message: 'No such session' });

    cb(sessionId, sessions[sessionId]);
};

var error = function(res) {
    return function(err) {
        res.json(500, { message: err.message || 'Unknown error' });
    };
};

var createId = function() {
    return crypto.randomBytes(20).toString('hex');
}

var parseBBox = function(rawArray) {
    var coords = [];

    for (var i in rawArray) {
        coords.push(parseFloat(rawArray[i]));
    }

    return coords;
}

app.get("/", function(req, res) {
    res.json(404, { message: 'Invalid service URL' });
});

// handlers for our API
app.get("/validate", function(req, res) {
    var pdalSession = new PdalSession();
    pdalSession.parse(req.body.pipeline, function(err) {
        pdalSession.destroy();
        if (err) console.log('Pipeline validation error:', err);
        res.json({ valid: err ? false : true });
    });
});

app.post("/create", function(req, res) {
    var pipelineId = req.body.pipelineId;
    var pipeline = req.body.pipeline;
    var pdalSession = pipelineIds[pipelineId] || new PdalSession();

    // Make sure to set these outside of the callback so that if another
    // request for this pipeline comes immediately after this one, it doesn't
    // create a new PdalSession and clobber our pipelineIds mapping.
    var sessionId = createId();
    sessions[sessionId]     = pdalSession;
    pipelineIds[pipelineId] = pdalSession;

    // It is safe to call create multiple times on a PdalSession, and it will
    // only actually be created one time.  Subsequent create calls will come
    // back immediately if the initial creation is complete, or if the initial
    // creation is still in progress, then the callback will be executed when
    // that call completes.
    pdalSession.create(pipeline, function(err) {
        if (err) {
            console.log('Error in CREATE:', err);
            return error(res)(err);
        }

        console.log('Created session:', sessionId);
        res.json({ sessionId: sessionId });
    });
});

app.delete("/sessions/:sessionId", function(req, res) {
    var sessionId = req.params.sessionId;
    if (sessions.hasOwnProperty(sessionId)) {
        delete sessions[sessionId];
    }
    return res.json({ message: 'Removed session ' + sessionId });
});

app.delete("/:pipelineId", function(req, res) {
    var sessionsRemoved = [];
    var pipelineId = req.params.pipelineId;

    if (pipelineIds.hasOwnProperty(pipelineId)) {
        for (var sessionId in sessions) {
            if (sessions[sessionId] === pipelineIds[pipelineId]) {
                sessionsRemoved.push(sessionId);
                delete sessions[sessionId];
            }
        }

        pipelineIds[pipelineId].destroy();
        delete pipelineIds[pipelineId];

        console.log('DESTROY pipelineId', pipelineId);
        return res.json({ message: 'Deleted', sessions: sessionsRemoved });
    }
    else {
        return res.json(
            400,
            { message: 'Pipeline not found' });
    }
});

app.get("/pointsCount/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        res.json({ count: pdalSession.getNumPoints() });
    });
});

app.get("/schema/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        res.json({ schema: pdalSession.getSchema() });
    });
});

app.get("/stats/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        res.json({ stats: pdalSession.getStats() });
    });
});

app.get("/srs/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        res.json({ srs: pdalSession.getSrs() });
    });
});

app.post("/cancel/:sessionId", function(req, res) {
    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        console.log('Got CANCEL request for session', sessionId);
        res.json({ 'cancelled': pdalSession.cancel(req.body.readId) });
    });
});

app.post("/read/:sessionId", function(req, res) {
    var args = req.body;

    var host = args.host;
    var port = parseInt(args.port);
    var schema = args.hasOwnProperty('schema') ? JSON.parse(args.schema) : { };

    console.log("session handler: /read/:" + JSON.stringify(schema, null, "    "));

    if (!host)
        return res.json(
            400,
            { message: 'Destination host needs to be specified' });

    if (!port)
        return res.json(
            400,
            { message: 'Destination port needs to be specified' });

    getSession(res, req.params.sessionId, function(sessionId, pdalSession) {
        console.log('read('+ sessionId + ')');

        var readHandler = function(
            err,
            readId,
            numPoints,
            numBytes,
            rasterize,
            xBegin,
            xStep,
            xNum,
            yBegin,
            yStep,
            yNum)
        {
            if (err) {
                console.log('Erroring read:', err);
                return res.json(400, { message: err });
            }
            else {
                if (rasterize) {
                    return res.json({
                        readId: readId,
                        numPoints: numPoints,
                        numBytes: numBytes,
                        rasterize: rasterize,
                        xBegin: xBegin,
                        xStep: xStep,
                        xNum: xNum,
                        yBegin: yBegin,
                        yStep: yStep,
                        yNum: yNum,
                        message:
                            'Request queued for transmission to ' +
                            host + ':' + port,
                    });
                }
                else {
                    return res.json({
                        readId: readId,
                        numPoints: numPoints,
                        numBytes: numBytes,
                        message:
                            'Request queued for transmission to ' +
                            host + ':' + port,
                    });
                }
            }
        };

        if (
            args.hasOwnProperty('start') ||
            args.hasOwnProperty('count') ||
            Object.keys(args).length == 4 ||
            (args.hasOwnProperty('schema') && Object.keys(args).length == 5)) {

            // Unindexed read - 'start' and 'count' may be omitted.  If either
            // of them exists, or if the only arguments are
            // host+port+cmd+session, then use this branch.

            var start = args.hasOwnProperty('start') ?
                parseInt(args.start) : 0;
            var count = args.hasOwnProperty('count') ?
                parseInt(args.count) : 0;

            if (start < 0) start = 0;
            if (count < 0) count = 0;

            pdalSession.read(
                    host,
                    port,
                    schema,
                    start,
                    count,
                    readHandler);
        }
        else if (
            args.hasOwnProperty('bbox') ||
            args.hasOwnProperty('depthBegin') ||
            args.hasOwnProperty('depthEnd') ||
            args.hasOwnProperty('rasterize')) {

            // Indexed read: quadtree query.
            var bbox =
                args.hasOwnProperty('bbox') ?
                    parseBBox(args.bbox) :
                    undefined;

            var depthBegin =
                args.hasOwnProperty('depthBegin') ?
                    parseInt(args.depthBegin) :
                    0;

            var depthEnd =
                args.hasOwnProperty('depthEnd') ?
                    parseInt(args.depthEnd) :
                    0;

            var rasterize =
                args.hasOwnProperty('rasterize') ?
                    parseInt(args.rasterize) :
                    0;

            pdalSession.read(
                    host,
                    port,
                    schema,
                    bbox,
                    depthBegin,
                    depthEnd,
                    rasterize,
                    readHandler);
        }
        else if (
            args.hasOwnProperty('radius') &&
            args.hasOwnProperty('x') &&
            args.hasOwnProperty('y')) {

            // Indexed read: point + radius query.

            var is3d = args.hasOwnProperty('z');
            var radius = parseFloat(args.radius);
            var x = parseFloat(args.x);
            var y = parseFloat(args.y);
            var z = is3d ? parseFloat(args.z) : 0.0;

            pdalSession.read(
                    host,
                    port,
                    schema,
                    is3d,
                    radius,
                    x,
                    y,
                    z,
                    readHandler);
        }
        else {
            console.log('Got bad read request', args);
            return res.json(
                400,
                { message: 'Invalid read command - bad params' });
        }
    });
});


disco.register("sh", function(err, service) {
    if (err) return console.log("Failed to start service:", err);

    var port = service.port;

    app.listen(port, function() {
        console.log('Session handler listening on port: ' + port);
    });
});

