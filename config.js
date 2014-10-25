var config = { };

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

    // If false, component will not run.
    enable: true,
};

// Webserver configuration.
config.web = {
    // Specify a port on which to listen.  If omitted or null, default is 8080.
    // IMPORTANT: This value must match the backend web port specified in
    // frontend-proxy/haproxy.cfg.
    port: 8080,

    // If false, component will not run.
    enable: true,
};

// Websocket handler configuration.
config.ws = {
    // Specify a port on which to listen.  If omitted or null, the process will
    // select an open port.
    port: null,

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

    // Time of inactivity per session handler, in minutes, after which to
    // destroy all traces of a session.
    //
    // If set to 0, sessions never expire and will never need reinitialization.
    // Only recommended if a small and well-known number of pipelines exist.
    //
    // Default: 60.
    sessionTimeoutMinutes: 60 * 48, // 2 days

    // Period, in seconds, to check for expired sessions and destroy them if
    // necessary.
    //
    // If set to 0, never check for expired sessions.  If sessionTimeoutMinutes
    // is non-zero, expirePeriodSeconds should also be non-zero.
    //
    // Default: 10
    expirePeriodSeconds: 60 * 10,   // 10 minutes

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
};

module.exports = config;

