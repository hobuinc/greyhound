// app.js
// Main entry point for pdal session delegation
//
process.title = 'gh_sh';
process.on('uncaughtException', function(err) {
    console.log('Caught at top level: ' + err);
});

process.on('uncaughtException', function(err) {
    console.log('Caught at top level: ' + err);
});

var express = require("express"),
    _ = require('lodash'),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser'),
    app = express(),

    net = require('net'),
    disco = require('../common').disco,
    console = require('clim')(),
    config = (require('../config').sh || { }),
    globalConfig = (require('../config').global || { }),
    mkdirp = require('mkdirp'),

    Session = require('./build/Release/session').Bindings,

    inputs = config.inputs,
    output = config.output,

    aws = (function() {
        var awsCfg = config.aws;
        var info = [];

        // TODO Parse the normal object in the addon.
        if (awsCfg) {
            info.push(awsCfg.url);
            info.push(awsCfg.bucket);
            info.push(awsCfg.access);
            info.push(awsCfg.hidden);

            console.log(
                'S3 serialization enabled for',
                awsCfg.url,
                '/',
                awsCfg.bucket);
        }
        else {
            console.log('S3 serialization disabled - no credentials supplied');
        }

        return info;
    })();

console.log('Read paths:', inputs);
console.log('Write path:', output);

app.use(methodOverride());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
app.use(app.router);

var pipelineIds = { }; // pipelineId -> session (one to one)

var error = function(res, err) {
    res.json(err.code, { message: err.message });
};

var getSession = function(name, cb) {
    var session;

    if (pipelineIds[name]) {
        session = pipelineIds[name];
    }
    else {
        session = new Session();
        pipelineIds[name] = session;
    }

    // Call every time, even if this name was found in our session mapping, to
    // ensure that initialization has finished before the session is used.
    try {
        console.log('Inputs:', inputs);
        session.create(name, aws, inputs, output, function(err) {
            if (err) delete pipelineIds[name];

            console.log('Create is back, err:', err);

            cb(err, session);
        });
    }
    catch (e) {
        delete pipelineIds[name];
        console.log('Caught exception in CREATE:', e);
        cb(e);
    }
};

var validateRasterSchema = function(schema) {
    // TODO Remove X/Y?
    // Schema must have X and Y dimensions, and at least one other dimension.
    var xFound = false, yFound = false, otherFound = false;
    for (var i = 0; i < schema.length; ++i) {
        if      (schema[i].name == 'X') xFound = true;
        else if (schema[i].name == 'Y') yFound = true;
        else otherFound = true;
    }

    return xFound && yFound && otherFound;
}

app.get("/", function(req, res) {
    res.status(404).json({ message: 'Invalid service URL' });
});

app.get('/exists/:resource', function(req, res) {
    getSession(req.params.resource, function(err) {
        return res.status(err ? 404 : 200).json({ });
    });
});

app.get("/numPoints/:plId", function(req, res) {
    getSession(req.params.plId, function(err, session) {
        if (err) return error(res, err);

        res.json({ numPoints: session.getNumPoints() });
    });
});

app.get("/schema/:plId", function(req, res) {
    getSession(req.params.plId, function(err, session) {
        if (err) return error(res, err);

        var dimensions = JSON.parse(session.getSchema()).dimensions;
        var schema = undefined;

        if (dimensions && _.isArray(dimensions)) {
            schema = new Array(dimensions.length);
            var dim;

            for (var i = 0; i < schema.length; ++i) {
                schema[i] = { };
                dim = dimensions[i];

                if (!dim.name || !dim.type || !dim.size) {
                    return res.json(400, { message: 'Invalid PDAL schema' });
                }

                schema[i].name = dim.name;
                schema[i].type = dim.type;
                schema[i].size = dim.size;
            }
        }

        res.json({ schema: schema });
    });
});

app.get("/stats/:plId", function(req, res) {
    getSession(req.params.plId, function(err, session) {
        if (err) return error(res, err);

        try {
            res.json({ stats: JSON.parse(session.getStats()) });
        }
        catch (e) {
            console.log('Invalid stats -', req.params.plId);
            return res.json(
                500,
                { message: 'Stats could not be parsed' });
        }
    });
});

app.get("/srs/:plId", function(req, res) {
    getSession(req.params.plId, function(err, session) {
        if (err) return error(res, err);

        res.json({ srs: session.getSrs() });
    });
});

app.post("/cancel/:plId", function(req, res) {
    getSession(res, req.params.plId, function(err, session) {
        if (err) return error(res, err);

        console.log('Got CANCEL request for session', sessionId);
        res.json({ 'cancelled': session.cancel(req.body.readId) });
    });
});

app.post("/read/:plId", function(req, res) {
    var address = req.body.address;
    var query = req.body.query;

    var host = address.host;
    var port = parseInt(address.port);
    var compress = !!query.compress;
    var schema = query.schema || [];

    console.log("session handler: /read/");

    if (!host)
        return res.json(
            400,
            { message: 'Controller host needs to be specified' });

    if (!port)
        return res.json(
            400,
            { message: 'Controller port needs to be specified' });

    // Prune our query to simplify the specialization decision tree.
    delete query.command;
    if (query.hasOwnProperty('compress')) delete query.compress;
    if (query.hasOwnProperty('schema')) delete query.schema;

    getSession(req.params.plId, function(err, session) {
        if (err) return error(res, err);

        console.log('read('+ req.params.plId + ')');

        var readHandler = function(
            err,
            readId,
            numPoints,
            numBytes,
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
                if (xNum && yNum) {
                    res.json({
                        readId: readId,
                        numPoints: numPoints,
                        numBytes: numBytes,
                        rasterMeta: {
                            xBegin: xBegin,
                            xStep: xStep,
                            xNum: xNum,
                            yBegin: yBegin,
                            yStep: yStep,
                            yNum: yNum
                        },
                        message:
                            'Request queued for transmission to ' +
                            host + ':' + port,
                    });
                }
                else {
                    res.json({
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

        // Send the results to the websocket-handler.
        var socket = net.createConnection(port, host);
        socket.on('close', function() {
            console.log('Socket closed');
        }).on('error', function(err) {
            // This is an expected occurence.  A cancel request
            // will cause the websocket-handler to forcefully reset
            // the connection.
            console.log('Socket force-closed:', err);
        });

        var dataHandler = function(err, data, done) {
            if (err) {
                socket.end();
            }
            else {
                socket.write(data);
                if (done) socket.end();
            }
        }

        if (
            query.hasOwnProperty('start') ||
            query.hasOwnProperty('count') ||
            Object.keys(query).length == 0) {

            // Unindexed read - 'start' and 'count' may be omitted.  If either
            // of them exists, or if the only arguments are
            // host+port+cmd+plId, then use this branch.
            console.log('    Got unindexed read request');

            var start =
                query.hasOwnProperty('start') ? parseInt(query.start) : 0;
            var count =
                query.hasOwnProperty('count') ? parseInt(query.count) : 0;

            if (start < 0) start = 0;
            if (count < 0) count = 0;

            session.read(
                    host,
                    port,
                    compress,
                    schema,
                    start,
                    count,
                    readHandler,
                    dataHandler);
        }
        else if (
            query.hasOwnProperty('bbox') &&
            query.hasOwnProperty('resolution')) {

            console.log('    Got generic raster read request');

            if (schema.hasOwnProperty('schema') &&
                !validateRasterSchema(schema)) {

                console.log(
                        'Invalid schema in raster request', schema);

                return res.json(
                    400,
                    { message: 'Bad schema - must contain X and Y' });
            }

            var bbox = query.bbox;
            var resolution = query.resolution;

            if (bbox.length != 4 || resolution.length != 2) {
                console.log('    Bad query in generic raster request', query);
                return res.json(
                    400,
                    { message: 'Invalid read command - bad params' });
            }

            session.read(
                host,
                port,
                compress,
                schema,
                bbox,
                resolution,
                readHandler,
                dataHandler);
        }
        else if (
            query.hasOwnProperty('bbox') ||
            query.hasOwnProperty('depthBegin') ||
            query.hasOwnProperty('depthEnd')) {

            console.log('    Got quad-tree depth range read request');

            // Indexed read: quadtree query.
            var bbox = query.bbox;
            var depthBegin =
                query.hasOwnProperty('depthBegin') ?
                    parseInt(query.depthBegin) :
                    0;

            var depthEnd =
                query.hasOwnProperty('depthEnd') ?
                    parseInt(query.depthEnd) :
                    0;

            session.read(
                    host,
                    port,
                    compress,
                    schema,
                    bbox,
                    depthBegin,
                    depthEnd,
                    readHandler,
                    dataHandler);
        }
        else if (query.hasOwnProperty('rasterize')) {
            var rasterize = parseInt(query.rasterize);

            console.log('    Got quad-tree single-level raster read request');

            if (schema.hasOwnProperty('schema') &&
                !validateRasterSchema(schema)) {

                console.log(
                        'Invalid schema in raster request', schema);

                return res.json(
                    400,
                    { message: 'Bad schema - must contain X and Y' });
            }

            session.read(
                    host,
                    port,
                    compress,
                    schema,
                    rasterize,
                    readHandler,
                    dataHandler);
        }
        else if (
            query.hasOwnProperty('radius') &&
            query.hasOwnProperty('x') &&
            query.hasOwnProperty('y')) {

            // Indexed read: point + radius query.
            console.log('    Got point-radius read request');

            var is3d = query.hasOwnProperty('z');
            var radius = parseFloat(query.radius);
            var x = parseFloat(query.x);
            var y = parseFloat(query.y);
            var z = is3d ? parseFloat(query.z) : 0.0;

            session.read(
                    host,
                    port,
                    compress,
                    schema,
                    is3d,
                    radius,
                    x,
                    y,
                    z,
                    readHandler,
                    dataHandler);
        }
        else {
            console.log('Got bad read request', query);
            return res.json(
                400,
                { message: 'Invalid read command - bad params' });
        }
    });
});

if (config.enable !== false) {
    disco.register("sh", config.port, function(err, service) {
        if (err) return console.log("Failed to start service:", err);

        var port = service.port;

        app.listen(port, function() {
            console.log('Session handler listening on port: ' + port);
        });
    });
}
else {
    process.exit(globalConfig.quitForeverExitCode || 42);
}

