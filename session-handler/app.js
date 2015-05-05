//
// Main entry point for Greyhound session delegation.
//

process.title = 'gh_sh';

process.on('uncaughtException', function(err) {
    console.log('Caught at top level: ' + err);
});

var express = require('express'),
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
    aws = config.aws;

console.log('Read paths:', inputs);
console.log('Write path:', output);
if (aws)
    console.log('S3 credentials enabled');
else
    console.log('S3 serialization disabled - no credentials supplied');

app.use(methodOverride());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
app.use(app.router);

var resourceIds = { }; // resource name -> session (one to one)

var error = function(res, err) {
    res.json(err.code, { message: err.message });
};

var getSession = function(name, cb) {
    var session;

    if (resourceIds[name]) {
        session = resourceIds[name];
    }
    else {
        session = new Session();
        resourceIds[name] = session;
    }

    // Call every time, even if this name was found in our session mapping, to
    // ensure that initialization has finished before the session is used.
    try {
        session.create(name, aws, inputs, output, function(err) {
            if (err) console.warn(name, 'could not be created');
            else     console.log (name, 'created');

            if (err) delete resourceIds[name];

            cb(err, session);
        });
    }
    catch (e) {
        delete resourceIds[name];
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

app.get('/', function(req, res) {
    res.status(404).json({ message: 'Invalid service URL' });
});

app.get('/exists/:resource', function(req, res) {
    getSession(req.params.resource, function(err) {
        return res.status(err ? 404 : 200).json({ });
    });
});

app.get('/numPoints/:resource', function(req, res) {
    getSession(req.params.resource, function(err, session) {
        if (err) return error(res, err);

        res.json({ numPoints: session.getNumPoints() });
    });
});

app.get('/schema/:resource', function(req, res) {
    getSession(req.params.resource, function(err, session) {
        if (err) return error(res, err);

        res.json({ schema: session.getSchema() });
    });
});

app.get('/stats/:resource', function(req, res) {
    getSession(req.params.resource, function(err, session) {
        if (err) return error(res, err);

        try {
            res.json({ stats: JSON.parse(session.getStats()) });
        }
        catch (e) {
            console.log('Invalid stats -', req.params.resource);
            return res.json(
                500,
                { message: 'Stats could not be parsed' });
        }
    });
});

app.get('/srs/:resource', function(req, res) {
    getSession(req.params.resource, function(err, session) {
        if (err) return error(res, err);

        res.json({ srs: session.getSrs() });
    });
});

app.get('/bounds/:resource', function(req, res) {
    getSession(req.params.resource, function(err, session) {
        if (err) return error(res, err);

        res.json({ bounds: session.getBounds() });
    });
});

app.post('/cancel/:resource', function(req, res) {
    getSession(res, req.params.resource, function(err, session) {
        if (err) return error(res, err);

        console.log('Got CANCEL request for session', sessionId);
        res.json({ 'cancelled': session.cancel(req.body.readId) });
    });
});

app.post('/read/:resource', function(req, res) {
    console.log('session handler: /read/');

    var address = req.body.address;
    var host = address.host;
    var port = parseInt(address.port);

    var query = req.body.query;

    var schema = query.schema || [];
    var compress = query.hasOwnProperty('compress') && !!query.compress;

    // Simplify our query decision tree for later.  Technically these values
    // are specified via query parameters, but they aren't relevant for
    // determining which type of 'read' to execute.
    delete query.schema;
    delete query.compress;

    if (!host || typeof host !== 'string') {
        console.log('Invalid host');
        return res.json(400, { message: 'Invalid host' });
    }

    if (!port) {
        console.log('Invalid port');
        return res.json(400, { message: 'Invalid port' });
    }

    try {
        schema = JSON.parse(schema);
    }
    catch (e) {
        return res.json(400, { message: 'Invalid schema' });
    }

    getSession(req.params.resource, function(err, session) {
        console.log('getSession(', req.params.resource, ') err:', err);
        if (err) return error(res, err);

        console.log('read('+ req.params.resource + ')');

        var socket;

        var readCb = function(
            err,
            readId,
            numPoints,
            numBytes,
            rasterMeta)
        {
            if (err) {
                console.log('Erroring read:', err);
                return res.json(400, { message: err.message });
            }
            else {
                // Send the results to the Controller.
                socket = net.createConnection(port, host);
                socket.on('close', function() {
                    console.log('Socket closed');
                }).on('error', function(err) {
                    // This is an expected occurence.  A cancel request
                    // will cause the Controller to forcefully reset
                    // the connection.
                    console.log('Socket force-closed:', err);
                });

                var response = {
                    readId: readId,
                    numPoints: numPoints,
                    numBytes: numBytes,
                    message:
                        'Request queued for transmission to ' +
                        host + ':' + port,
                };

                if (rasterMeta) {
                    response.rasterMeta = rasterMeta;
                }

                res.json(response);
            }
        };

        var dataCb = function(err, data, done) {
            if (err) {
                socket.end();
            }
            else {
                socket.write(data);
                if (done) socket.end();
            }
        }

        session.read(host, port, schema, compress, query, readCb, dataCb);
    });
});

if (config.enable !== false) {
    disco.register('sh', config.port, function(err, service) {
        if (err) return console.log('Failed to start service:', err);

        var port = service.port;

        app.listen(port, function() {
            console.log('Session handler listening on port: ' + port);
        });
    });
}
else {
    process.exit(globalConfig.quitForeverExitCode || 42);
}

