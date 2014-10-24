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

These dependencies must be installed separately and independently.

 - `PDAL`_
 - `Node.js`_ 10.29 or greater
 - `Redis`_ server
 - `HAProxy`_
 - C++11 compiler
 - `Boost`_ library 1.55 or greater

.. _`PDAL`: http://www.pdal.io/index.html
.. _`Node.js`: http://nodejs.org/
.. _`Redis`: http://redis.io/
.. _`Haproxy`: http://www.haproxy.org/
.. _`Boost`: http://www.boost.org/

Global NPM Dependencies
-------------------------------------------------------------------------------

NPM dependencies may be installed via the Node.js package manager "npm", which is included with an installation of Node.js.

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

The WebSocket handler interprets incoming requests and performs appropriate forwarding to a `Database Handler`_ and `Session Handler`_, then returns responses back to the calling client.  The WebSocket handler is also responsible for load balancing back-end calls to the other components and for maintaining `Session Affinity`_ mappings from a unique session to its associated session handler.

Database Handler
-------------------------------------------------------------------------------

The database handler performs the lookup from a ``pipeline ID`` to a `PDAL pipeline`_ file when a new Greyhound session is created.

Database drivers may be implemented for various back-end formats.  Greyhound supports an HTTP driver and a MongoDB driver (for database driver selection, see `Internal Configuration`_).

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

Greyhound building and installation is accomplished via Makefile.  The basic steps required to install Greyhound are:

 - ``git clone https://github.com/hobu/greyhound.git && cd greyhound``
 - ``make``
 - ``make install``
 - Configure the Greyhound ``init.d`` services to be auto-executable.
 - Reboot to auto-launch or manually launch Greyhound ``init.d`` services.

Makefile targets
-------------------------------------------------------------------------------

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

**HAProxy** is configured via ``/var/greyhound/frontend-proxy/haproxy.cfg``.  Key entries are:

 - ``backend ws`` - Must match Hipache's port.
 - ``backend web`` - If the Greyhound web server is enabled, this entry must match ``config.web.port`` in ``config.js``.
 - ``frontend fe`` - The ``bind`` parameter specifies the only public-facing incoming port of Greyhound, so all incoming requests must target this port, and any firewall on the Greyhound server must leave this port open.

**Hipache** is configured via ``/var/greyhound/frontend-proxy/hipache-config.json``.  Hipache receives incoming WebSocket traffic from HAProxy and routes this traffic to a `WebSocket Handler`_.  Key configuration entries are:

 - ``server.port`` - Must match the ``backend ws`` port specified in HAProxy's configuration.
 - ``server.workers`` - Number of worker threads to route WebSocket requests.
 - ``driver`` - Must match Greyhound's Redis server location, port, and database selection entry.  WebSocket handler instances register themselves with this Redis store via the `Distribution Handler`_ to make themselves available to Hipache.

Use-Cases
-------------------------------------------------------------------------------

TODO - discuss how specific deployers might actually configure stuff
 - Web demo, few pipelines w/ eternal or very long life
 - HTTP DB server deployment, many pipelines w/ short life
 - Multiple vs single session handlers

Serving
===============================================================================

TODO - init.d setup
TODO - status command
TODO - process entries (gh_* and 5x nodejs)

Performance
===============================================================================

Session Affinity
-------------------------------------------------------------------------------

TODO - session affinity, load balancing, and shared session basics


Logging
===============================================================================

TODO - /var/log/greyhound/gh_*

