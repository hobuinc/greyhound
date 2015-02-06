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

    PdalSession = require('./build/Release/pdalBindings').PdalBindings,
    Affinity = require('../common/affinity').Affinity,
    affinity = new Affinity(disco),

    serialCompress =
        (config.serialCompress == undefined) ? true : !!config.serialCompress,

    // serialPaths[0] is to be used for writing new serialized entries.
    // serialPaths[0..n] are to be searched when looking for serialized entries.
    //
    // If AWS credentials are specified, then S3 will be used for writing and
    // serialPaths will be read-only unless S3 is unreachable.
    serialPaths = (function() {
        if (!config.serialAllowed) return undefined;

        var paths = [];
        var defaultSerialPath = '/var/greyhound/serial/';

        var tryCreate = function(dir) {
            try {
                mkdirp.sync(dir);
                serialPath = dir;
                return true;
            }
            catch (err) {
                console.error('Could not create serial path', dir);
                return false;
            }
        };

        if (config.serialPaths &&
            config.serialPaths.length &&
            typeof serialPaths[0] === 'string') {

            if (tryCreate(serialPaths[0])) {
                paths = serialPaths;
                paths.push(defaultSerialPath);
            }
        }

        if (!paths || !paths.length) {
            if (tryCreate(defaultSerialPath)) {
                paths.push(defaultSerialPath);
            }
        }

        if (!paths.length) {
            console.error('Serialization disabled');
        }

        return paths.length ? paths : undefined;
    })(),

    aws = (function() {
        var awsCfg = config.aws;
        var info = [];

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


console.log('Serial paths:', serialPaths);

// configure express application
app.use(methodOverride());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
app.use(app.router);

var pipelineIds = { }; // pipelineId -> pdalSession (one to one)

var getSession = function(res, plId, cb) {
    if (!pipelineIds[plId])
        res.json(404, { message: 'No such pipeline' });

    cb(pipelineIds[plId]);
};

var error = function(res) {
    return function(err) {
        res.json(500, { message: err.message || 'Unknown error' });
    };
};

var validateRasterSchema = function(schema) {
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
    res.json(404, { message: 'Invalid service URL' });
});

app.post("/create", function(req, res) {
    var pipelineId = req.body.pipelineId;
    var pathData = req.body.pathData;
    var bbox = req.body.bbox;

    console.log(':session-handler:CREATE', req.body.pipelineId);

    if (_.isArray(pathData) && !bbox) {
        return res.json(400, { message: 'No bbox in multi specification' });
    }

    var pdalSession = pipelineIds[pipelineId] || new PdalSession();

    // Make sure to set these outside of the callback so that if another
    // request for this pipeline comes immediately after this one, it doesn't
    // create a new PdalSession and clobber our pipelineIds mapping.
    pipelineIds[pipelineId] = pdalSession;

    // It is safe to call create multiple times on a PdalSession, and it will
    // only actually be created one time.  Subsequent create calls will come
    // back immediately if the initial creation is complete, or if the initial
    // creation is still in progress, then the callback will be executed when
    // that call completes.
    pdalSession.create(
        pipelineId,
        pathData,
        serialCompress,
        aws,
        serialPaths,
        bbox,
        function(err)
    {
        if (err) {
            delete pipelineIds[pipelineId];

            console.log('Error in CREATE:', err);
            return error(res)(err);
        }

        console.log('Successfully finished CREATE:', pipelineId);

        res.json({ });
    });
});

app.get("/numPoints/:plId", function(req, res) {
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        res.json({ numPoints: pdalSession.getNumPoints() });
    });
});

app.get("/schema/:plId", function(req, res) {
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        var dimensions = JSON.parse(pdalSession.getSchema()).dimensions;
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
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        try {
            res.json({ stats: JSON.parse(pdalSession.getStats()) });
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
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        res.json({ srs: pdalSession.getSrs() });
    });
});

app.get("/fills/:plId", function(req, res) {
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        res.json({ fills: pdalSession.getFills() });
    });
});

app.get("/serialize/:plId", function(req, res) {
    if (!config.serialAllowed || (!aws && !serialPaths)) {
        return res.json(400, { message: 'Serialization disabled' });
    }

    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        pdalSession.serialize(aws, serialPaths, function(err) {
            if (err) console.log('ERROR during serialization:', err);
        });
        res.json({ message: 'Serialization task launched' });
    });
});

app.post("/cancel/:plId", function(req, res) {
    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
        console.log('Got CANCEL request for session', sessionId);
        res.json({ 'cancelled': pdalSession.cancel(req.body.readId) });
    });
});

app.post("/read/:plId", function(req, res) {
    var args = req.body;

    var host = args.host;
    var port = parseInt(args.port);
    var compress = !!args.compress;
    var schema = args.schema || [];

    console.log("session handler: /read/");

    if (!host)
        return res.json(
            400,
            { message: 'Destination host needs to be specified' });

    if (!port)
        return res.json(
            400,
            { message: 'Destination port needs to be specified' });

    // Prune our args to simplify the specialization decision tree.
    delete args.command;
    delete args.host;
    delete args.port;
    if (args.hasOwnProperty('compress')) delete args.compress;
    if (args.hasOwnProperty('schema')) delete args.schema;

    getSession(res, req.params.plId, function(pdalSession) {
        if (!pdalSession) return;
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
            if (err) socket.end();

            socket.write(data);
            if (done) socket.end();
        }

        if (
            args.hasOwnProperty('start') ||
            args.hasOwnProperty('count') ||
            Object.keys(args).length == 0) {

            // Unindexed read - 'start' and 'count' may be omitted.  If either
            // of them exists, or if the only arguments are
            // host+port+cmd+plId, then use this branch.
            console.log('    Got unindexed read request');

            var start = args.hasOwnProperty('start') ? parseInt(args.start) : 0;
            var count = args.hasOwnProperty('count') ? parseInt(args.count) : 0;

            if (start < 0) start = 0;
            if (count < 0) count = 0;

            pdalSession.read(
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
            args.hasOwnProperty('bbox') &&
            args.hasOwnProperty('resolution')) {

            console.log('    Got generic raster read request');

            if (schema.hasOwnProperty('schema') &&
                !validateRasterSchema(schema)) {

                console.log(
                        'Invalid schema in raster request', schema);

                return res.json(
                    400,
                    { message: 'Bad schema - must contain X and Y' });
            }

            var bbox = args.bbox;
            var resolution = args.resolution;

            if (bbox.length != 4 || resolution.length != 2) {
                console.log('    Bad args in generic raster request', args);
                return res.json(
                    400,
                    { message: 'Invalid read command - bad params' });
            }

            pdalSession.read(
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
            args.hasOwnProperty('bbox') ||
            args.hasOwnProperty('depthBegin') ||
            args.hasOwnProperty('depthEnd')) {

            console.log('    Got quad-tree depth range read request');

            // Indexed read: quadtree query.
            var bbox = args.bbox;
            var depthBegin =
                args.hasOwnProperty('depthBegin') ?
                    parseInt(args.depthBegin) :
                    0;

            var depthEnd =
                args.hasOwnProperty('depthEnd') ?
                    parseInt(args.depthEnd) :
                    0;

            pdalSession.read(
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
        else if (args.hasOwnProperty('rasterize')) {
            var rasterize = parseInt(args.rasterize);

            console.log('    Got quad-tree single-level raster read request');

            if (schema.hasOwnProperty('schema') &&
                !validateRasterSchema(schema)) {

                console.log(
                        'Invalid schema in raster request', schema);

                return res.json(
                    400,
                    { message: 'Bad schema - must contain X and Y' });
            }

            pdalSession.read(
                    host,
                    port,
                    compress,
                    schema,
                    rasterize,
                    readHandler,
                    dataHandler);
        }
        else if (
            args.hasOwnProperty('radius') &&
            args.hasOwnProperty('x') &&
            args.hasOwnProperty('y')) {

            // Indexed read: point + radius query.
            console.log('    Got point-radius read request');

            var is3d = args.hasOwnProperty('z');
            var radius = parseFloat(args.radius);
            var x = parseFloat(args.x);
            var y = parseFloat(args.y);
            var z = is3d ? parseFloat(args.z) : 0.0;

            pdalSession.read(
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
            console.log('Got bad read request', args);
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

