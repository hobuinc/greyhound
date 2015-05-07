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
    // AWS credentials - specified in the file: credentials.js, if it exists.
    // If not supplied, S3 capabilities will be disabled.
    aws: awsCredentials,

    // Where to find unindexed pointcloud source files and indexed
    // subdirectories, searched in order.
    inputs: [
        '/vagrant/examples/data'
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

