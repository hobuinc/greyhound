(function() {
    var SQLiteDriver = function() {
        var sqlite3 = require('sqlite3').verbose(),
            db = null;

        this.initialize = function(options, cb) {
            // TODO - only allows one handler
            db = new sqlite3.Database(':memory:');

            // cb(err);
        }

        this.put = function(pipeline, cb) {
            // cb(err, pipelineId);
        }

        this.retrieve = function(pipelineId, cb) {
            // cb(err, pipeline);
        }
    };

    module.exports.SQLiteDriver = SQLiteDriver;
})();

