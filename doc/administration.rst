===============================================================================
Greyhound - Usage and Deployment
===============================================================================

:author: Connor Manning
:email: connor@hobu.co
:date: 10/23/2014

Overview
===============================================================================

Greyhound is a distributed server architecture that abstracts and maintains persistent sessions for PDAL pipelines used for real-time querying of point cloud data.

A Greyhound server is separated into independent components with well-defined tasks.  Each of these components is independently scalable to accomodate a variety of use-cases.

Dependencies
===============================================================================

External Dependencies
-------------------------------------------------------------------------------

These dependencies must be installed separately and independently from Greyhound.

Dependencies:
 - `PDAL`_ compiled with `LazPerf`_ compression enabled (``-DWITH_LAZPERF=ON``)
 - `Node.js`_ 10.29 or greater
 - `Redis`_ server
 - `HAProxy`_
 - C++11 compiler
 - `Boost`_ library 1.55 or greater
 - `SQLite3`_ C++ library

.. _`PDAL`: http://www.pdal.io/index.html
.. _`Node.js`: http://nodejs.org/
.. _`Redis`: http://redis.io/
.. _`Haproxy`: http://www.haproxy.org/
.. _`Boost`: http://www.boost.org/
.. _`SQLite3`: https://www.sqlite.org/capi3ref.html
.. _`LazPerf`: https://github.com/verma/laz-perf

Global NPM Dependencies
-------------------------------------------------------------------------------

NPM dependencies may be installed via the Node.js package manager "npm", which is included with an installation of Node.js.

NPM dependencies:
 - ``hipache``
 - ``node-gyp``
 - ``nodeunit`` (unit testing module - optional)

These packages may be installed after Node.js is installed with ``npm install -g hipache node-gyp nodeunit``.  The ``-g`` flag specifies global installation.

Architecture
===============================================================================

TODO - Diagram

Front-end Proxy
-------------------------------------------------------------------------------

The front-end proxy is responsible for routing external commands to a `WebSocket Handler`_, or to the `Web Server`_.  The front-end proxy is made up of two pieces: HAProxy and Hipache.

An incoming message first reaches HAProxy, which determines whether the message is an HTTP or a WebSocket message.  HAProxy routes the message to the `Web Server`_ if it is an HTTP message.

If the message is using the WebSocket protocol, it is routed to Hipache, which routes the request to one of the `WebSocket Handler`_ instances.  Hipache's knowledge of the available WebSocket handlers comes from the self-registration of these handlers with the `Distribution Handler`_ component.

Distribution Handler
-------------------------------------------------------------------------------

When any Greyhound component is instantiated, that component registers its component type and address with Greyhound's Redis store.  The distribution handler is responsible for watching for `WebSocket Handler`_ registrations and deregistrations, and updating the state of the WebSocket handler services with Hipache's Redis store.

WebSocket Handler
-------------------------------------------------------------------------------

The WebSocket handler interprets incoming requests and performs appropriate forwarding to a `Database Handler`_ and `Session Handler`_, then returns responses back to the calling client.  The WebSocket handler is also responsible for load balancing back-end calls to the other components and for maintaining session affinity mappings from a unique session to its associated session handler.

Database Handler
-------------------------------------------------------------------------------

The database handler performs the lookup from a ``pipeline ID`` to a `PDAL pipeline`_ file when a new Greyhound session is created.

Database drivers may be implemented for various back-end formats.  Greyhound supports an HTTP driver and a MongoDB driver (for database driver selection, see `Internal Configuration`_).

Drivers:
 - HTTP driver - this driver that there is a read-only HTTP server external to Greyhound containing a mapping of ``pipeline IDs`` to pipelines.
 - Mongo driver - used for standalone operation for testing and demonstration purposes.  This driver supports requests to add a pipeline mapping to the database.

.. _`PDAL pipeline`: http://www.pdal.io/pipeline.html

Session Handler
-------------------------------------------------------------------------------

The session handler contains an abstraction layer to PDAL to provide "session" functionality.  Within this component, multiple Greyhound sessions using the same pipeline may share the same PDAL session for faster access without re-execution of the PDAL pipeline.

This is the only component of Greyhound that uses C++, due to its interface with PDAL.

Web Server
-------------------------------------------------------------------------------

The Greyhound web server is an optional component, and is meant mainly for demonstration and testing purposes.  The web server sends a simple client application that exercises Greyhound's session life-cycle from the caller's browser, using URL parameters to issue various ``read`` queries and displays the result in WebGL.

In a production Greyhound environment targeting dedicated clients, it is recommended that the web server should be disabled (see `Internal Configuration`_).

Obtaining, Building, and Installing
===============================================================================

Greyhound building and installation is accomplished via Makefile.

Minimum installation steps:
 - ``git clone https://github.com/hobu/greyhound.git && cd greyhound``
 - ``make``
 - ``make install``
 - Configure the Greyhound ``init.d`` services to be auto-executable.
 - Reboot to auto-launch or manually launch Greyhound ``init.d`` services.

Makefile targets
-------------------------------------------------------------------------------

Targets:
 - ``required`` - Install NPM dependencies for each Greyhound component and build the C++ session handler.  This is the default ``make`` target.
 - ``all`` - Perform ``make required`` and then build the C++ examples.
 - ``cpp`` - Build the C++ session-handler via ``node-gyp``.
 - ``npm`` - Install NPM dependencies for each Greyhound component as specified by the ``package.json`` file of each component.
 - ``examples`` - Build C++ examples.
 - ``test`` - Run all unit tests.  Greyhound must be running locally and ``nodeunit`` must be globally installed.
 - ``clean`` - Clean executables from the session-handler and C++ examples.
 - ``install`` - Install Greyhound service scripts into ``/etc/init.d``, copy necessary Greyhound executables to ``/var/greyhound/``, and install the ``greyhound`` utility command into ``/usr/bin/``.  By default, ``install`` will not include a MongoDB service, required for standalone operation.
 - ``install STANDALONE=TRUE`` - Install Greyhound including a MongoDB service for standalone Greyhound operation.
 - ``uninstall`` - Remove all traces of Greyhound installation (including log files).

Greyhound Administration
===============================================================================

After Greyhound installation, the ``init.d`` services of Greyhound must be registered for auto-launch, the method for which is OS-dependent.  The Greyhound lauchers installed into ``/etc/init.d/`` contain ``chkconfig`` lines to ensure the proper launch order.  If launch order is changed during auto-launch registration, note that the `Front-end Proxy`_ and the Mongo service (if using standalone mode) should be configured to launch prior to all other Greyhound services.

All Greyhound services are prefixed with ``gh_``, followed by an abbreviated service name.

Service names:
 - ``gh_fe`` - Front-end proxy.
 - ``gh_mongo`` - MongoDB launcher, for standalone mode only.
 - ``gh_ws`` - WebSocket handler.
 - ``gh_db`` - Database handler.
 - ``gh_dist`` - Distribution handler.
 - ``gh_sh`` - Session handler.
 - ``gh_web`` - Web server.

|

After auto-launch registration, services will launch on reboot.  Individual services may also be manually controlled with ``/etc/init.d/gh_<COMPONENT> {start|stop}``.  See `Commanding Greyhound`_ for more information.

Commanding Greyhound
-------------------------------------------------------------------------------

A utility command called ``greyhound`` is provided with the Greyhound installation.  This command provides simple access to some common Greyhound tasks.  Commands are of the format ``greyhound <COMMAND>``

Commands:
 - ``start`` - Launch all Greyhound ``init.d`` services (requires root).
 - ``stop`` - Stop all Greyhound ``init.d`` services (requires root).
 - ``status`` - Display running Greyhound services and each of their listening ports.
 - ``auto`` - An *Ubuntu-specific* command to register Greyhound services for auto-launch on boot.
 - ``rmauto`` - An *Ubuntu-specific* command to unregister Greyhound services from auto-launching.

Greyhound Processes
-------------------------------------------------------------------------------

Greyhound creates two processes for each running component - the component itself, and a monitor for that component which relaunches the component in the case of a fatal error.  The names of the component processes are the names specified in `Greyhound Administration`_ , the names of the monitors are these same names with ``_monitor`` appended.  So a session handler will appear as two processes named ``gh_sh`` and ``gh_sh_monitor``.

Hipache's workers, the number of which is specified in the `Front-end Proxy Settings`_, appear as processes named ``nodejs``.

Logging
-------------------------------------------------------------------------------

Greyhound logs are written to separate files for each component in ``/var/log/greyhound/``.

Internal Configuration
===============================================================================

Configuration file
-------------------------------------------------------------------------------

After installation, Greyhound may be configured through a JavaScript configuration file located at ``/var/greyhound/config.js``.  This file specifies parameters for each individual Greyhound component, and the configuration is used by Greyhound at startup (so changes to this file require Greyhound to be relaunched).

Each component configuration allows a ``port`` value to be defined, on which the specified server component will listen.  It is recommended that each ``port`` value be set to ``null`` to allow the component to choose a free port.  None of the ``port`` parameters specified in ``config.js`` should be accessible to the outside, as these are all back-end ports.  The only exception is the port of the web server which, although it is a back-end port, must be well-known.  For more information on web server settings, and on public-facing port definitions, see `Front-end Proxy Settings`_.

Each component has its own set of configuration parameters, and defaults are given and described in detail in ``config.js``.  Important configurable parameters include database driver selection and options, session sharing parameters, and pipeline expiration settings.

Front-end Proxy Settings
-------------------------------------------------------------------------------

The *front-end proxy* consists of HAProxy and Hipache.  The HAProxy component is the first stop for incoming requests, and determines by the connection protocol (WebSocket or HTTP) whether to route to the back-end web server or to a WebSocket handler.

**HAProxy** is configured via ``/var/greyhound/frontend-proxy/haproxy.cfg``.

HAProxy key configuration entries:
 - ``backend ws`` - Must match Hipache's port.
 - ``backend web`` - If the Greyhound web server is enabled, this entry must match ``config.web.port`` in ``config.js``.
 - ``frontend fe`` - The ``bind`` parameter specifies the only public-facing incoming port of Greyhound, so all incoming requests must target this port, and any firewall on the Greyhound server must leave this port open.

**Hipache** is configured via ``/var/greyhound/frontend-proxy/hipache-config.json``.  Hipache receives incoming WebSocket traffic from HAProxy and routes this traffic to a `WebSocket Handler`_.

Hipache key configuration entries:
 - ``server.port`` - Must match the ``backend ws`` port specified in HAProxy's configuration.
 - ``server.workers`` - Number of worker threads to route WebSocket requests.
 - ``driver`` - Must match Greyhound's Redis server location, port, and database selection entry.  WebSocket handler instances register themselves with this Redis store via the `Distribution Handler`_ to make themselves available to Hipache.

Use-Cases
-------------------------------------------------------------------------------

Configuration may vary considerably depending on the purpose and expected use-cases of the Greyhound deployment.

As an example, consider a production environment with a large pipeline database and sporadic use of a small percentage of pipelines, where a specific pipeline is only accessed briefly by a small number of users.  In this scenario, we would want a short session timeout to avoid wasting memory maintaining an idle open session.  Let's also assume we want the fastest response time possible once the sessions are executed, so we'll prefer to have a small number of concurrent users per session.  This requires multiple session handlers to be enabled.

``config.js`` sample settings:
 - ``config.web.enable: false`` - Disable web server for production environment.
 - ``config.db.type: 'http'`` - Use an external database web server API for pipeline retrieval.  ``config.db.options`` must be set accordingly.
 - ``config.ws.softSessionShareMax: 4`` - After 4 concurrent users of a single pipeline on a session handler, put new users of the same pipeline on a different session handler.
 - ``config.ws.hardSessionShareMax: 6`` - If the same pipeline has 4 concurrent users on *every* session handler, allow additional users to share with them until each session handler has 6 simultaneous users of the pipeline.  After that, don't allow any new sessions to be created with that pipeline.
 - ``config.ws.sessionTimeoutMinutes: 15`` - Destroy PDAL sessions after 15 minutes of inactivity.

|

Another possible deployment scenario is a demonstration environment for a Greyhound client with a small and fixed number of pipelines.  An example would be a demonstration of a rendering client backed by Greyhound.  In this example we might never want to block access to a pipeline, and we might allow a large number of users to share a session.

``config.js`` sample settings:
 - ``config.web.enable: true`` - For testing Greyhound back-end.
 - ``config.db.type: 'mongo'`` - Use a standalone Greyhound environment with its own database.  ``config.db.options`` must be set accordingly.
 - ``config.ws.softSessionShareMax: 64`` - Allow a high number of concurrent users of a pipeline before offloading to a new session handler.
 - ``config.ws.hardSessionShareMax: 0`` - Place no limits on the maximum concurrent user cap.  Performance might suffer with large amounts of concurrent users.
 - ``config.ws.sessionTimeoutMinutes: 0`` - Never internally destruct a PDAL session since this scenario has only a small number of pipelines - keep them ready in memory from their first access onward.


