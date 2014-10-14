var config = { };

// Database handler configuration.
config.db = {
    // Supported types are 'mongo', 'oracle', and 'sqlite'.  Default is 'mongo'.
    type: 'mongo',

    // The 'options' object will be passed to the database driver's
    // initialize function.  Necessary information will vary per driver, but
    // all information needed to initialize/connect should be contained here.
    options: {
        // Database connection info.  URL follows the form:
        //      domain:port/name
        domain: 'mongodb://localhost',
        port: 21212,
        name: 'greyhound',
    }
};

// Websocket handler configuration.
config.ws = {
    // After this many concurrent users on a single session within a single
    // session handler, offload any further requests to a different session
    // handler if possible.  This will cause reinitialization on the new
    // session handler. If all other handlers are similarly loaded or no other
    // handlers exist, we can load beyond this value up to hardSessionShareMax.
    //
    // If set to 0, all users of the same pipeline ID will share a single
    // session on a single session handler.
    softSessionShareMax: 16,

    // After this many concurrent users on a single session within a single
    // session handler, disallow any further users from sharing this session.
    //
    // If set to 0, no hard limit is placed on the number of concurrent users of
    // a session.
    hardSessionShareMax: 64,

    // Time of inactivity per session handler, in minutes, after which to
    // destroy all traces of a session.
    //
    // If set to 0, sessions never expire and will never need reinitialization.
    // Only recommended if a small and well-known number of pipelines exist.
    sessionTimeoutMinutes: 60 * 48, // 2 days

    // Period, in seconds, to check for expired sessions and destroy them if
    // necessary.
    //
    // If set to 0, never check for expired sessions.  If sessionTimeoutMinutes
    // is non-zero, expirePeriodSeconds should also be non-zero.
    expirePeriodSeconds: 60 * 10,   // 10 minutes
};

module.exports = config;

