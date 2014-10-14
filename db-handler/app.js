// Main entry point for the pipeline database
process.title = 'gh_db';

var express = require('express')
  , app = express()
  , methodOverride = require('method-override')
  , bodyParser = require('body-parser')
  , disco = require('../common').disco
  , console = require('clim')()
  , MongoDriver = require('./drivers/mongo').MongoDriver

  , config = (require('../config').db || { })
  , type = (config.type || 'mongo')
  , options = (config.options || { })
  , driver = null
  ;

switch (type) {
    case 'grid':
        break;
    case 'mongo':
    default:
        driver = new MongoDriver();
}

// Configure express server.
app.use(methodOverride());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());
app.use(express.errorHandler({
    dumpExceptions: true,
    showStack: true
}));
app.use(app.router);

var error = function(res) {
    return function(err) {
        console.log("Responding with a 500 ERROR:", err);
        res.json(500, { message: err.message || 'Unknown error' });
    };
};

var safe = function(res, f) {
    try {
        f();
    }
    catch(e) {
        console.log("Request failed!", e);
        error(res)(e);
    }
}

app.get("/", function(req, res) {
    res.json(404, { message: 'Invalid service URL' });
});

// Handle a 'put' request.
app.post("/put", function(req, res) {
    driver.put(req.body.pipeline, function(err, pipelineId) {
        if (err)
            return error(res)(err);

        // Respond with database ID of the inserted pipeline.
        return res.json({ id: pipelineId });
    });
});

app.get("/retrieve", function(req, res) {
    safe(res, function() {
        var pipelineId = req.body.pipelineId;
        console.log("/retrieve with pipelineId:", pipelineId);

        driver.retrieve(pipelineId.toString(), function(err, foundPipeline) {
            if (err)
                return error(res)(err);
            else if (!foundPipeline)
                return error(res)('Could not retrieve pipeline');

            console.log(
                "/retrieve with pipelineId:",
                pipelineId,
                "successful");

            return res.json({ pipeline: foundPipeline });
        });
    });
});

// Set up the database and start listening.
disco.register('db', function(err, service) {
    if (err) return console.log("Failed to start service:", err);

    driver.initialize(options, function(err) {
        if (err) {
            console.log('db-handler initialize failed:', err);
        }
        else {
            app.listen(service.port, function() {
                console.log(
                    'Database handler listening on port: ' + service.port);
            });
        }
    });
});

