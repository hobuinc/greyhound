// Main entry point for the pipeline database
process.title = 'gh_db';

var express = require('express')
  , app = express()
  , methodOverride = require('method-override')
  , bodyParser = require('body-parser')
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
    app.use(methodOverride());
    app.use(bodyParser.urlencoded({ extended: true }));
    app.use(bodyParser.json());
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
            if (err) {
                fs.close(fd, function() {
                    return cb(err);
                });
            }

            db = new sqlite3.Database(dbFile);

            db.serialize(function() {
                db.run(
                    'CREATE TABLE IF NOT EXISTS pipelines (' +
                        'pipelineId TEXT PRIMARY KEY,' +
                        'filename TEXT' +
                    ')');

                db.close();

                fs.close(fd, function() {
                    return cb(null);
                });
            });
        });
    });
}

var put = function(pipeline, cb) {
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

            fs.open(dbFile, 'a', function(err, fd) {
                if (err)
                    return cb(err);

                db = new sqlite3.Database(dbFile, sqlite3.OPEN_READWRITE);

                // TODO Need to detect collisions.
                var stmt = db.prepare(
                    "INSERT OR IGNORE INTO pipelines VALUES (?,?);");
                stmt.run(hash, hash);
                stmt.finalize();

                db.close();

                fs.close(fd, function() {
                    // Return the database ID for the newly inserted file.
                    return cb(null, hash)
                });
            });
        });
    });
}

var retrieve = function(pipelineId, cb) {
    fs.open(dbFile, 'r', function(err, fd) {
        db = new sqlite3.Database(dbFile, sqlite3.OPEN_READONLY);

        db.all(
            "SELECT rowid as id, filename FROM pipelines WHERE pipelineId = ?;",
            pipelineId,
            function(err, rows) {
                db.close();

                if (err)
                    return cb(err);
                else if (rows.length > 1)
                    return cb(new Error("Database results invalid"));
                else if (rows.length == 0)
                    return cb(new Error("PipelineId " + pipelineId + " not found"));

                var filename = rows[0].filename;

                fs.readFile(dataDir + filename, 'utf8', function(err, data) {

                    fs.close(fd, function() {
                        // Return the database ID for the newly inserted file.
                        return cb(err, data)
                    });
                });
            }
        );
    });
}

var validatePipeline = function(pipeline) {
    // TODO - Use PDAL to validate pipeline.
    return typeof pipeline === 'string' && pipeline.length !== 0;
}

app.get("/", function(req, res) {
	res.json(404, { message: 'Invalid service URL' });
});

// Handle a 'put' request.
app.post("/put", function(req, res) {
    var pipeline = req.body.pipeline;

    if (validatePipeline(pipeline)) {
        put(pipeline, function(err, pipelineId) {
            if (err)
                return error(res)(err);

            // Respond with database ID of the inserted pipeline.
            return res.json({ id: pipelineId });
        });
    }
    else {
        console.log('Pipeline validation failed');
        return error(res)(new Error('Invalid pipeline'));
    }
});

app.get("/retrieve", function(req, res) {
    var pipelineId = req.body.pipelineId;

    retrieve(pipelineId, function(err, foundPipeline) {
        if (err)
            return error(res)(err);

        return res.json({ pipeline: foundPipeline });
    });
});

// Set up the database and start listening.
configureDb(function(err) {
    if (err) {
        console.log('Database setup error: ', err)
    }
    else {
        var port = ports.register('db@0.0.1');
        app.listen(port, function() {
            console.log('DB handler listening on port: ' + port);
        });
    }
});

