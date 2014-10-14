(function() {
    var MongoDriver = function() {
        var mongo = require('mongoskin'),
            crypto = require('crypto'),
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

        this.put = function(pipeline, cb) {
            console.log("db.put");

            // Hash the pipeline as the database key.
            var hash = crypto.createHash('md5').update(pipeline).digest("hex");
            var entry = { id: hash, pipeline: pipeline };

            db.collection('pipelines').findAndModify(
                { id: hash },
                { },
                { $setOnInsert: entry },
                { 'new': true, 'upsert': true },
                function(err, result) {
                    console.log("    :got put result");
                    if (err) console.log("        :err:", err);
                    return cb(err, result.id);
                }
            );
        }

        this.retrieve = function(pipelineId, cb) {
            console.log("db.retrieve");
            var query = { 'id': pipelineId };

            console.log("    :querying for pipelines");
            db.collection('pipelines').findOne(query, function(err, entry) {
                if (err) console.log("    db.collection.findOne:", err);
                if (!err && (!entry || !entry.hasOwnProperty('pipeline')))
                    return cb('Invalid entry retreived');

                return cb(err, entry.pipeline);
            });
        }
    };

    module.exports.MongoDriver = MongoDriver;
})();

