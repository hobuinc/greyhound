(function() {
    var HttpDriver = function() {
        var prefix = null,
            postfix = null;

        this.initialize = function(options, cb) {
            prefix = options.prefix || '';
            postfix = options.postfix || '';
            return cb(null);
        }

        this.put = function(options, cb) {
            return cb('PUT not supported for this driver');
        }

        this.retrieve = function(pipelineId, cb) {
            console.log('Retrieving pipeline ID:', pipelineId);

            http.get(prefix + pipelineId + postfix, function(res) {
                console.log('    Got response:' + res.statusCode);
                cb(null, res);
            }).on('error', function(e) {
                console.log('    Got error:' + e.message);
            });
        }
    };

    module.exports.HttpDriver = HttpDriver;
})();

