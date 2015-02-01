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

// Database handler configuration.
config.db = {
    // Supported types are 'mongo' and 'http'.
    type: 'mongo',

    // Specify a port on which to listen.  If omitted or null, the process will
    // select an open port.
    port: null,

    // The 'options' object will be passed to the database driver's
    // initialize function.  Necessary information will vary per driver, but
    // all information needed to initialize/connect should be contained here.
    options: {
        // Used by mongo driver only:
        // Database connection info.  URL follows the form:
        //      domain:port/name
        domain: 'mongodb://localhost',
        port: 21212,
        name: 'greyhound',

        // Used by http driver only:
        // Greyhound will format an HTTP GET request to a URL of the format:
        //      <prefix><pipeline ID><postfix>
        prefix: '',
        postfix: '',
    },

    // If false, component will not run.
    enable: true,
};

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

    // Specify whether Greyhound can serialize indexed pipelines to disk or
    // to AWS S3 (if credentials are supplied) for shorter creation times and
    // less RAM usage.
    //
    // If set to true and AWS credentials are supplied, pipelines will be
    // serialized to S3 instead of the local filesystem.
    serialAllowed: true,

    // If serialAllowed is true, these paths will be used for serialization.
    // Rules:
    //      For reading:
    //        - Search these paths, in order, for a serialized Greyhound file.
    //        - If '/var/greyhound/serial' does not exist in this list, search
    //          there if no matches were found in this list.
    //
    //      For writing:
    //        - If this list is populated, try to use the first entry as the
    //          writing location.  If this list is empty or the first location
    //          could not be validated or created, try to use
    //          '/var/greyhound/serial'.  If neither of these locations works,
    //          serialization will be disabled completely.
    //        - Although Greyhound will only write to the first entry or the
    //          default, Greyhound will check all listed directories for the
    //          presence of a pipeline that is requested to be serialized.
    //          The pipeline will not be written again if it exists in any
    //          of the listed directories.
    serialPaths: [
        // '/var/greyhound/serial',
    ],

    // If true or omitted, serialized pipelines will be written with LazPerf
    // compression.  If false, serialization will be uncompressed.  Previously
    // compressed/serialized pipelines will be able to be decompressed by
    // Greyhound regardless of this value, which only governs how data is
    // written.
    serialCompress: true,

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

    // After this many concurrent users on a single session within a single
    // session handler, offload any further requests to a different session
    // handler if possible.  This will cause reinitialization on the new
    // session handler. If all other handlers are similarly loaded or no other
    // handlers exist, we can load beyond this value up to hardSessionShareMax.
    //
    // This parameter has no effect if only one session handler exists.
    //
    // If set to 0, all users of the same pipeline ID will share a single
    // session on a single session handler.
    //
    // Default: 16.
    softSessionShareMax: 16,

    // After this many concurrent users on a single session within a single
    // session handler, disallow any further users from sharing this session.
    //
    // If set to 0, no hard limit is placed on the number of concurrent users of
    // a session.
    //
    // Default: 0.
    hardSessionShareMax: 0,

    // Time of inactivity for a single client session to remain active before
    // being deleted.  After this time has expired, but before
    // pipelineTimeoutMinutes has elapsed, a client can recreate a session
    // simply and quickly.
    //
    // Expiration checks are amortized, so this configuration entry represents
    // the minimum time before expiration occurs.
    //
    // Default: 5.
    sessionTimeoutMinutes: 5,

    // Time of inactivity per session handler, in minutes, after which to
    // destroy all traces of a PDAL pipeline.
    //
    // If set to 0, initialized pipelines never expire and will never need
    // reinitialization. Only recommended if a small and well-known number of
    // pipelines exist.
    //
    // Expiration checks are amortized, so this configuration entry represents
    // the minimum time before expiration occurs.
    //
    // Default: 30.
    pipelineTimeoutMinutes: 30,

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

