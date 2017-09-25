===============================================================================
Greyhound - Administration and Deployment
===============================================================================

:author: Connor Manning
:email: connor@hobu.co
:date: 09/22/2016

Overview
===============================================================================

Greyhound is an HTTP server that provides dynamic level-of-detail point cloud streaming.

Docker installation
===============================================================================

Fetch the latest docker image:

``docker pull connormanning/greyhound``

Run the server:
``docker run -it -p 8080:8080 connormanning/greyhound``

Native installation
===============================================================================

External Dependencies
-------------------------------------------------------------------------------

These dependencies must be installed prior to installing Greyhound.

Dependencies:
 - C++11 compiler
 - `PDAL`_ compiled with `LazPerf`_ compression enabled (``-DWITH_LAZPERF=ON``)
 - `Simple-Web-Server`_
 - `Node.js`_ 4.0 or greater (for unit testing only - not a Greyhound requirement)

.. _`PDAL`: http://www.pdal.io/index.html
.. _`LazPerf`: https://github.com/verma/laz-perf
.. _`Simple-Web-Server`: https://github.com/eidheim/Simple-Web-Server
.. _`Node.js`: http://nodejs.org/

Installing
-------------------------------------------------------------------------------

Native greyhound installation is accomplished via cmake.  On Unix:

::

    mkdir build && cd build
    cmake -G "Unix Makefiles" ..
    make && make install

Then to run Greyhound with a default configuration, simply run ``greyhound``.

Testing
-------------------------------------------------------------------------------

::

    # From <greyhound-root>/build:
    # Generate testing resource.
    ../scripts/generate-test-data.sh
    # Run Greyhound.
    ./greyhound/greyhound
    # Then, in new tab:
    cd ../test
    npm install
    npm run test

Hello world
===============================================================================

With Greyhound running, browse to http://speck.ly/?s=http://localhost:8080/&r=autzen or http://potree.entwine.io/data/custom.html?s=localhost:8080&r=autzen to view a small publicly hosted point cloud served from your locally-running Greyhound server.

Configuration
===============================================================================

Greyhound accepts a JSON configuration file at launch with the ``-c`` command line flag: ``greyhound -c /var/greyhound/config.json``.

A simple configuration file might look like this:

::

    {
        "cacheSize": "1 GB",
        "paths": ["/opt/data", "~/greyhound", "http://greyhound.io"],
        "resourceTimeoutMinutes": 30,
        "http": {
            "port": 8080,
            "headers": {
                "Cache-Control":                  "public, max-age=300",
                "Access-Control-Allow-Origin":    "*",
                "Access-Control-Allow-Methods":   "GET,PUT,POST,DELETE"
            }
        }
    }

Configuration settings
-------------------------------------------------------------------------------

- ``cacheSize``: The cache size for Greyhound's data chunks.  This is not a maximal amount of memory that Greyhound may use, but is merely correlated with the amount of memory Greyhound will consume since it represents only a single piece of Greyhound's internal data usage.  This field may be specified as a number of bytes, but may also be a specified as a string containing a qualifier like ``MB`` or ``GB``.
- ``paths``: An array of strings representing the paths in which Greyhound will search, in order, for data to stream.  Defaults are ``/opt/data`` for easy Docker mapping, ``~/greyhound`` for a default native location, and ``http://greyhound.io`` for sample data.  Local paths, HTTP(s) URLs, and S3 paths (assuming proper credentials exist) are supported.
- ``resourceTimeoutMinutes``: The number of minutes after which Greyhound can erase local storage for a given resource.  Default: ``30``.
- ``http.port``: Port on which to listen for HTTP requests.  If ``null`` or missing, HTTP requests will be disabled.  Default: ``8080``.
- ``http.securePort``: Port on which to listen for HTTPS requests.  If ``null`` or missing, HTTPS requests will be disabled.  If this value is specified, ``http.keyFile`` and ``http.certFile`` must also be present.  Default: ``undefined``.
- ``http.keyFile``: Path to HTTPS key file.
- ``http.certFile``: Path to HTTPS certificate file.
- ``http.headers``: An object with string-to-string key-value pairs representing headers that will be placed on all outbound response data from Greyhound.  Common use-cases for this field are CORS headers and cache control.  Defaults to the values shown in the sample configuration above.

Authentication settings
-------------------------------------------------------------------------------

Greyhound supports the use of a cookie-based external authentication server to authenticate users to their requested resources before serving any data related to that resource.  This is achieved by asking an external authentication server for access to a resource based on some configured cookie name.

This places some domain restrictions on your hosting.  This is because the relevant cookie will only be sent to Greyhound if Greyhound and the authentication server are on the same top-level domain, and that the cookie domain is set loosely enough to be sent to the Greyhound server.

For the examples below, we'll assume that Greyhound is hosted at https://server.greyhound.io, and that the authentication server is https://auth.greyhound.io

- ``auth.path``: A string URL to which Greyhound will proxy requests.  Greyhound will add ``/<resource>`` to this path when requesting authentication.  If a user requests a resource called ``the-moon``, with our example settings, the authentication request will be sent to ``https://hello.io/the-moon``.

- ``auth.cookieName``: The name of the cookie used as a unique ID by the authentication server.  This may be a login token, unique ID, a special Greyhound identifier, and may even be a secure cookie.  Greyhound will forward this cookie in its request to the authentication server, and will cache this value to identify future requests in accordance with the authentication cache settings.

- ``auth.cacheMinutes``: This field specifies the maximum amount of time, in minutes, that Greyhound should cache the authentication server response for each unique user.  If this field is a number, then both allow (``2xx``) and deny (all other) responses will be cached for this many minutes.  This field can also be set to an object with ``good`` and ``bad`` keys, which will specify separately the duration for which a successful response and an unsuccessful response may be cached.

Examples
===============================================================================

Configuration with HTTP disabled, HTTPS enabled, and external authentication
-------------------------------------------------------------------------------

::

    {
        "cacheSize": "1 GB",
        "paths": ["s3://my-app/entwine/"],
        "resourceTimeoutMinutes": 30,
        "http": {
            "port": null,
            "headers": {
                "Cache-Control":                  "public, max-age=300",
                "Access-Control-Allow-Origin":    "greyhound.io",
                "Access-Control-Allow-Methods":   "GET,PUT,POST,DELETE"
            },
            "securePort": 443,
            "keyFile": "/opt/keys/greyhound-key.pem",
            "certFile": "/opt/keys/greyhound-cert.pem"
        },
        "auth": {
            "path": "https://auth.greyhound.io",
            "cookieName": "greyhound-user-id",
            "cacheMinutes": {
                "good": 10,
                "bad": 1
            }
        }
    }

