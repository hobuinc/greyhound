var fs = require('fs');
var config = { };

var awsCredentials = (function() {
    if (fs.existsSync(__dirname + '/' + 'credentials.js')) {
        return require(__dirname + '/' + 'credentials').aws;
    }
    else {
        return null;
    }
})();

// Controller configuration.
config.cn = {
    // If queryLimits values are configured to be outside the ranges specified
    // below, they will be silently clipped or grown to fall within the valid
    // range.
    queryLimits: {
        // Number of outstanding simultaneous queries.  Must be in the
        // inclusive range [8, 128].
        concurrentQueries: 16,

        // Maximum number of indexed chunks allowed to be fetched per query.
        // Smaller numbers result in faster query response times.  Must be no
        // less than 4.
        chunksPerQuery: 8,

        // Number of previously fetched chunks that may be held in the cache.
        // If this value is less than concurrentQueries * chunksPerQuery, the
        // cache may be overrun under heavy load, which causes 'internal server
        // error' responses to valid queries.  Must be no less than 16.
        chunkCacheSize: 128
    },

    // AWS credentials - specified in the file: credentials.js, if it exists.
    // If not supplied, S3 capabilities will be disabled.
    aws: awsCredentials,

    // Where to find unindexed pointcloud source files and indexed
    // subdirectories, searched in order.
    inputs: [
        '/vagrant/examples/data',
        's3://entwine'
        // , 's3://my-index-bucket'
    ],

    // Directory for Greyhound to serialize its indexed data.
    //
    // Default: '/var/greyhound/serial'
    output: '/var/greyhound/serial',    // 's3://greyhound-index-bucket'

    http: {
        // True to support an HTTP interface.
        enable: true,

        // Set to true to serve static files at /data/ for testing/verification.
        enableStaticServe: true,

        // HTTP headers to be applied to Greyhound responses.  Likely uses
        // include CORS settings and cache control.
        headers: {
            'Cache-Control':                  'public, max-age=86400',
            'Access-Control-Allow-Origin':    '*',
            'Access-Control-Allow-Methods':   'GET,PUT,POST,DELETE'
        },

        // Specify a port on which to listen.  If omitted or null, default is
        // 8081. This is not a public-facing port - haproxy will route to this
        // port for HTTP requests.
        //
        // IMPORTANT: If a frontend proxy is used, this value must match the
        // backend web port specified in frontend-proxy/haproxy.cfg.
        port: 8081,
    },

    ws: {
        // True to support a websocket interface.
        enable: true,

        // Specify a port on which to listen.  If omitted or null, default is
        // 8082. This is not a public-facing port - haproxy will route to this
        // port for websocket connections.
        //
        // IMPORTANT: If a frontend proxy is used, this value must match the
        // backend web port specified in frontend-proxy/haproxy.cfg.
        port: 8082,
    },

    // Time of inactivity per session handler, in minutes, after which to
    // free the allocations used to maintain a Greyhound resource.
    //
    // If set to 0, initialized resources never expire and will never need
    // reinitialization. Only recommended if a small and well-known number of
    // resources exist.
    //
    // Expiration checks are amortized, so this configuration entry represents
    // the minimum time before expiration occurs.
    //
    // Default: 30.
    resourceTimeoutMinutes: 30,
};

module.exports = config;

