var mongo = require('mongoskin'),
    path = 'mongodb://localhost:21212/greyhound',
    args = { native_parser: true },
    db = mongo.db(path, args),
    crypto = require('crypto');

(function() {
    var MongoDriver = function() {
        this.preLaunch = function(cb) {
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

