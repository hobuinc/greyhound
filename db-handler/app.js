// Main entry point for the pipeline database

var express = require('express')
  , app = express()
  , seaport = require('seaport')
  , ports = seaport.connect('localhost', 9090)
  , sqlite3 = require('sqlite3')
  , fs = require('fs')
  , mkdirp = require('mkdirp')
  , index = 0
  , db = null
  , dbDir = __dirname + '/db/'
  , dbFile = dbDir + 'pipelines.db' // Database of indices->filenames
  , dataDir = dbDir + 'data/';      // Directory of actual pipeline files

app.configure(function() {
    app.use(express.methodOverride());
    app.use(express.bodyParser());
    app.use(express.errorHandler({
        dumpExceptions: true,
        showStack: true
    }));
    app.use(app.router);
});

var error = function(res) {
	return function(err) {
		res.json(500, { message: err.message || 'Unknown error' });
	};
};

function configureDb(cb) {
    mkdirp(dbDir, function(err) {
        if (err)
            return cb(err);

        fs.open(dbFile, 'a', function(err, fd) {
            if (err)
                return cb(err)

            db = new sqlite3.Database(dbFile);

            db.serialize(function() {
                db.run(
                    'CREATE TABLE IF NOT EXISTS pipelines (' +
                        'pipelineId INTEGER PRIMARY KEY,' +
                        'filename TEXT' +
                    ')');
            });
        });
    });

    // TODO: Should close DB here, re-open when needed.
    // For now the sqlite3 DB is open for the lifetime of the db-handler.
    // Retrieve operations should open in read-only.
    return cb(null);
}

// TODO: Hash the pipeline as the key?
var put = function(pipeline, cb)
{
    mkdirp(dataDir, function(err) {
        if (err)
            return cb(err);

        var filename = 'f' + index + '.xml';
        var fs = require('fs');

        // Write pipeline to the data directory successfully before adding
        // its index to the db.
        fs.writeFile(dataDir + filename, pipeline, function(err) {
            if(err)
                return cb(err);

            // TODO Need to detect collisions.
            var stmt = db.prepare("INSERT OR IGNORE INTO pipelines VALUES (?,?);");
            stmt.run(index, filename);
            stmt.finalize();

            // For now, return the index that maps to the inserted pipeline.
            return cb(err, index)
        });
    });
}

var retrieve = function(pipelineId, cb)
{
    db.all(
        "SELECT rowid as id, filename FROM pipelines WHERE pipelineId = ?;",
        pipelineId,
        function(err, rows) {
            console.log("SELECTED: ", rows);

            if (err)
                return cb(err);
            else if (rows.length !== 1)
                // TODO Create error.
                return 1;

            var filename = rows[0].filename;

            fs.readFile(dataDir + filename, 'utf8', function(err, data) {
                return cb(err, data);
            });
        });
}

// Handle a 'put' request.
app.post("/put", function(req, res) {
    index++;
    console.log('Got PUT request in DB');
    var pipeline = req.body.pipeline;

    put(pipeline, function(err, pipelineId) {
        if (err)
            return error(res)(err);

        // Return database ID of the inserted pipeline.
        res.json({ id: pipelineId });
    });
});

app.get("/retrieve", function(req, res) {
    var pipelineId = req.body.pipelineId;
    console.log('Got RETRIEVE request in DB: ', pipelineId);

    retrieve(pipelineId, function(err, foundPipeline) {
        if (err)
            return error(res)(err);

        res.json({ pipeline: foundPipeline });
    });
});

// Set up the database and start listening.
configureDb(function(err) {
    if (err) {
        console.log('Database setup error: ', err)
    } else {
        var port = ports.register('db@0.0.1');
        app.listen(port, function() {
            console.log('DB handler listening on port: ' + port);
        });
    }
});

