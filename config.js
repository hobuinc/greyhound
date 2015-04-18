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

config.dist = {
    // If false, component will not run.
    enable: true,
};

// Session handler configuration.
config.sh = {
    // Specify a port on which to listen.  If omitted or null, the process will
    // select an open port.
    port: null,

    // AWS credentials - specified in the file: credentials.js, if it exists.
    // If not supplied, S3 capabilities will be disabled.
    aws: awsCredentials,

    // Where to find unindex pointcloud source files.  If supplied, the AWS
    // remote path will be searched first.
    inputs: [
        '/vagrant/examples/data/'
    ],

    // TODO Allow S3 here.
    // Directory for Greyhound to serialize its indexed data.
    //
    // Default: '/var/greyhound/serial'
    output: '/var/greyhound/serial',

    // If false, component will not run.
    enable: true,
};

// Controller configuration.
config.cn = {
    ws: {
        // True to support a websocket interface.
        enable: true,

        // Specify a port on which to listen.  If omitted or null, the process
        // will select an open port.
        port: null,
    },

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
        // port for non-websocket connections.
        //
        // IMPORTANT: If a frontend proxy is used, this value must match the
        // backend web port specified in frontend-proxy/haproxy.cfg.
        port: 8081,
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

    // If false, component will not run.
    enable: true,
};

config.global = {
    // Components that wish to terminate without restarting should exit with
    // this error code.  The typical case for this would be if the component is
    // disabled.  Any monitor program should not restart its child process if
    // the child returns with this exit code.
    //
    // Should not be greater than 255, and should not be a reserved exit code
    // or a code with any other meaning, so avoid 1-2, 64-78, 126-165, and 255.
    //
    // Default if omitted or null: 42.
    quitForeverExitCode: 42,

    // Number of times to relaunch components if they encounter an unexpected
    // error.
    //
    // To relaunch eternally, omit this parameter or set it to null.  A value
    // of zero means that components will exit forever if an unhandled error is
    // encountered.  A positive value specifies the number of times to relaunch
    // the component.  A negative value is treated the same way as a zero.
    //
    // Default: null.
    relaunchAttempts: null,
};

module.exports = config;

