(function() {
    var MongoDriver = function() {
        var mongo = require('mongoskin'),
            crypto = require('crypto'),
            fs = require('fs'),
            _ = require('lodash'),
            db = null;

        this.initialize = function(options, cb) {
            console.log("db.initialize");

            if (!options.domain) {
                return cb('options must include database path');
            }

            var path =
                options.domain +
                ':' +
                (options.port || 21212) +
                '/' +
                (options.name || 'greyhound');

            db = mongo.db(path, { native_parser: true }),

            db.collection('pipelines').ensureIndex(
                [[ 'id', 1 ]],              // Ensure index by ID.
                { unique: true },           // IDs are unique.
                function(err, replies) {
                    cb(err);
                }
            );
        }

        this.put = function(path, cb) {
            console.log("db.put", path);

            var insert = function(hash, path, cb) {
                var entry = { id: hash, pipeline: path };

                db.collection('pipelines').findAndModify(
                    { id: hash },
                    { },
                    { $setOnInsert: entry },
                    { 'new': true, 'upsert': true },
                    function(err, result) {
                        console.log("    :got put result:");
                        if (err) console.log("        :err:", err);
                        return cb(err, hash/*result.id*/);
                    }
                );
            }

            if (_.isString(path)) {
                fs.open(path, 'r', function(err, fd) {
                    if (err) return cb(err);

                    var buffer = new Buffer(1024);
                    fs.read(fd, buffer, 0, 1024, 0, function(err, num) {
                        // Hash the pipeline as the database key.
                        var hash = crypto.createHash('md5')
                                .update(buffer.toString('utf-8', 0, num))
                                .digest("hex");

                        insert(hash, path, cb);
                    });
                });
            }
            else {
                var hash = crypto.randomBytes(16).toString('hex');
                insert(hash, path, cb);
            }
        }

        this.retrieve = function(pipelineId, cb) {
            console.log("db.retrieve");
            var query = { 'id': pipelineId };

            console.log("    :querying for pipelines");
            db.collection('pipelines').findOne(query, function(err, entry) {
                if (err) console.log("    db.collection.findOne:", err);
                if (!err && (!entry || !entry.hasOwnProperty('pipeline')))
                    return cb(404);

                return cb(err, entry.pipeline);
            });
        }
    };

    module.exports.MongoDriver = MongoDriver;
})();

