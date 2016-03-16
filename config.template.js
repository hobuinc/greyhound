{
    "queryLimits": {
        // Maximum number of indexed chunks allowed to be fetched per query.
        // Smaller numbers result in faster query response times.  Must be no
        // less than 4.
        "chunksPerQuery": 16,

        // Number of previously fetched chunks that may be held in the cache.
        // Must be no less than 16.
        "chunkCacheSize": 128
    },

    // Where to find unindexed pointcloud source files and indexed
    // subdirectories, searched in order.
    "paths": [
        "/opt/data"
        // , "s3://my-index-bucket"
    ],

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
    "resourceTimeoutMinutes": 30,

    "http": {
        // Set to true to serve static files at /data/ for testing/verification.
        "enableStaticServe": true,

        // HTTP headers to be applied to Greyhound responses.  Likely uses
        // include CORS settings and cache control.
        "headers": {
            "Cache-Control":                  "public, max-age=86400",
            "Access-Control-Allow-Origin":    "*",
            "Access-Control-Allow-Methods":   "GET,PUT,POST,DELETE"
        },

        // If null, no HTTP interface will be supported.
        "port": 80,

        // If keyFile and certFile are both non-null, and securePort is set
        // below, HTTPS will be supported.
        //
        // If credentials are named key.pem and cert.pem and placed at
        // Greyhound"s root directory, key.pem and cert.pem are accepted below.
        // Otherwise, absolute paths are required.
        "keyFile": "/opt/keys/greyhound/key.pem",
        "certFile": "/opt/keys/greyhound/cert.pem",

        // If null, no HTTPS interface will be supported.
        "securePort": 443
    },

    "ws": {
        // If null, no websocket interface will be supported.
        "port": 8989
    },

    // Greyhound supports the use of an external authentication server to
    // authenticate users to supplied resources before serving.  This is
    // achieved by proxying an entire Greyhound request to the auth path
    // specified below, postfixed with "/<resource>", which is the current
    // resource that the user is attempting to view.
    //
    // Depending on your authentication method(s), this may place some domain
    // restrictions on your hosting.  For example, if your authentication
    // server uses cookies for auth, those cookies will only survive Greyhound"s
    // proxying if Greyhound and the auth server are on the same top-level
    // domain, and that the cookie domain is set loosely enough to be sent to
    // the Greyhound server.
    //
    // Authentication is not supported over the websocket interface.
    "auth": {
        // Where to proxy Greyhound requests for authentication.  The
        // authentication server should send a 2xx status code to allow access
        // to this resource, or any other status if not.  An error status code
        // will be returned to the client unmodified.
        //
        // Sample flow:
        //          path: "me.com/auth"
        //
        //  - Client requests greyhound-domain.com/resource/autzen/info
        //  - Greyhound proxies request to me.com/auth/autzen
        //  - If response is 2xx, user has access until "good" auth timeout
        //  - Otherwise, user is denied access until "bad" auth timeout
        "path": "external-auth-server.my-domain.com/authenticate",

        // Greyhound will use the external application"s session cookies for its
        // session caching, and map this cookie to the auth server"s response.
        "cookieName": "my-application-session-id",

        // Specify how long Greyhound caches responses for both successful and
        // unsuccessful authentication attempts.  If set to a number instead of
        // an object, that number will be used for both "good" and "bad" auth
        // caching.
        //
        // Minimum values: 1.
        "cacheMinutes": {
            "good": 1,
            "bad": 1
        }
    }
}

