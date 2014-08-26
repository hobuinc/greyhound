// Main entry point for the pipeline database
process.title = 'gh_db';

var express = require('express')
  , app = express()
  , methodOverride = require('method-override')
  , bodyParser = require('body-parser')
  , seaport = require('seaport')
  , ports = seaport.connect('localhost', 9090)
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
    var query = { 'id': pipelineId };

    db.collection('pipelines').findOne(query, function(err, entry) {
        if (!err && (!entry || !entry.hasOwnProperty('pipeline')))
            return cb('Invalid entry retreived');

        return cb(err, entry.pipeline);
    });
}

var error = function(res) {
	return function(err) {
		res.json(500, { message: err.message || 'Unknown error' });
	};
};

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
    var pipelineId = req.body.pipelineId;

    retrieve(pipelineId.toString(), function(err, foundPipeline) {
        if (err)
            return error(res)(err);
        else if (!foundPipeline)
            return error(res)('Could not retrieve pipeline');

        return res.json({ pipeline: foundPipeline });
    });
});

db.collection('pipelines').ensureIndex(
        [[ 'id', 1 ]],              // Ensure index by ID.
        { unique: true },           // IDs are unique.
        function(err, replies) { });

// Set up the database and start listening.
var port = ports.register('db@0.0.1');
app.listen(port, function() {
    console.log('Database handler listening on port: ' + port);
});

