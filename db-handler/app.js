// Main entry point for the pipeline database

var express = require('express')
  , app = express()
  , seaport = require('seaport')
  , ports = seaport.connect('localhost', 9090)
  , sqlite3 = require('sqlite3')
  , fs = require('fs')
  , mkdirp = require('mkdirp')
  , db = null
  , dbDir = './db/'
  , dbFile = dbDir + 'pipelines.db' // Database of indices->filenames
  , dataDir = dbDir + 'data/';       // Directory of actual pipeline files

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
                        'id INTEGER PRIMARY KEY,' +
                        'filename TEXT' +
                    ')');
            });
        });
    });

    // TODO: Should close DB here, re-open when needed.
    return cb(null);
}

// TODO: Hash the pipeline as the key?
var index = 1;
function put(pipeline, cb)
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

            var stmt = db.prepare("INSERT INTO pipelines VALUES (?,?)");
            stmt.run(index, filename);
            stmt.finalize();

            index++;

            return cb(err, filename)
        });
    });
}

app.post("/put", function(req, res) {
    console.log('Got PUT request in DB');
    var pipeline = req.body.pipeline;
    var key = -1;

    put(pipeline, function(err, filename) {
        if (err)
            console.log(err);
            // TODO
            //return error(res)(err);

        res.json({ id: key });
    });
});

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

