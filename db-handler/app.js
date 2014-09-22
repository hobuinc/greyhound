// Main entry point for the pipeline database
process.title = 'gh_db';

var express = require('express')
  , app = express()
  , methodOverride = require('method-override')
  , bodyParser = require('body-parser')
  , disco = require('../common').disco
  , crypto = require('crypto')
  , mongo = require('mongoskin')
  , db = mongo.db('mongodb://localhost:21212/greyhound', { native_parser: true })
  ;

app.configure(function() {
    app.use(methodOverride());
    app.use(bodyParser.urlencoded({ extended: true }));
    app.use(bodyParser.json());
    app.use(express.errorHandler({
        dumpExceptions: true,
        showStack: true
    }));
    app.use(app.router);
});

var put = function(pipeline, cb) {
    // Hash the pipeline as the database key.
    var hash = crypto.createHash('md5').update(pipeline).digest("hex");
    var entry = { id: hash, pipeline: pipeline };

    db.collection('pipelines').findAndModify(
        { id: hash },
        { },
        { $setOnInsert: entry },
        { 'new': true, 'upsert': true },
        function(err, result) {
            return cb(err, result.id);
        }
    );
}

var retrieve = function(pipelineId, cb) {
    console.log("db.retrieve");
    var query = { 'id': pipelineId };

    console.log("    :querying for pipelines");
    db.collection('pipelines').findOne(query, function(err, entry) {
        console.log("    db.collection.findOne:", err, entry);
        if (!err && (!entry || !entry.hasOwnProperty('pipeline')))
            return cb('Invalid entry retreived');

        return cb(err, entry.pipeline);
    });
}

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
    put(req.body.pipeline, function(err, pipelineId) {
        if (err)
            return error(res)(err);

        // Respond with database ID of the inserted pipeline.
        return res.json({ id: pipelineId });
    });
});

app.get("/retrieve", function(req, res) {
    safe(res, function() {
        var pipelineId = req.body.pipelineId;
        console.log("/retrieve with pipeline:", pipelineId);

        retrieve(pipelineId.toString(), function(err, foundPipeline) {
            console.log("    retrieve:", err, foundPipeline);
            if (err)
                return error(res)(err);
            else if (!foundPipeline)
                return error(res)('Could not retrieve pipeline');

            console.log(pipelineId, "->", foundPipeline);
            return res.json({ pipeline: foundPipeline });
        });
    });
});

db.collection('pipelines').ensureIndex(
        [[ 'id', 1 ]],              // Ensure index by ID.
        { unique: true },           // IDs are unique.
        function(err, replies) { });

// Set up the database and start listening.
disco.register('db', function(err, service) {
    if (err) return console.log("Failed to start service:", err);
    app.listen(service.port, function() {
        console.log('Database handler listening on port: ' + service.port);
    });
});

