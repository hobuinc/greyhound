// Main entry point for the pipeline database

var express = require('express')
  , app = express()
  , seaport = require('seaport')
  , ports = seaport.connect('localhost', 9090)
  , sqlite3 = require('sqlite3')
  , fs = require('fs')
  , crypto = require('crypto')
  , mkdirp = require('mkdirp')
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
                        'pipelineId TEXT PRIMARY KEY,' +
                        'filename TEXT' +
                    ')');
            });
        });
    });

    // TODO: Should close DB (and DB file) here, re-open when needed.
    // For now the sqlite3 DB is open for the lifetime of the db-handler.
    // Retrieve operations should open in read-only.
    return cb(null);
}

var put = function(pipeline, cb)
{
    mkdirp(dataDir, function(err) {
        if (err)
            return cb(err);

        // Hash the pipeline as the database key.
        var hash = crypto.createHash('md5').update(pipeline).digest("hex");

        // Write pipeline to the data directory successfully before adding it
        // to the database.
        fs.writeFile(dataDir + hash, pipeline, function(err) {
            if(err)
                return cb(err);

            // TODO Need to detect collisions.
            var stmt = db.prepare("INSERT OR IGNORE INTO pipelines VALUES (?,?);");
            stmt.run(hash, hash);
            stmt.finalize();

            // Return the database ID for the newly inserted file.
            return cb(err, hash)
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
            else if (rows.length > 1)
                return cb(new Error("Database results invalid"));
            else if (rows.length == 0)
                return cb(new Error("PipelineId " + pipelineId + " not found"));

            var filename = rows[0].filename;

            fs.readFile(dataDir + filename, 'utf8', function(err, data) {
                return cb(err, data);
            });
        });
}

// Handle a 'put' request.
app.post("/put", function(req, res) {
    console.log('Got PUT request in DB');
    var pipeline = req.body.pipeline;

    put(pipeline, function(err, pipelineId) {
        if (err)
            return error(res)(err);

        // Respond with database ID of the inserted pipeline.
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

