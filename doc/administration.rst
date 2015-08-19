===============================================================================
Greyhound - Usage and Deployment
===============================================================================

:author: Connor Manning
:email: connor@hobu.co
:date: 08/17/2015

Overview
===============================================================================

Greyhound is a server architecture that provides dynamic level-of-detail point cloud streaming.

Dependencies
===============================================================================

External Dependencies
-------------------------------------------------------------------------------

These dependencies must be installed separately and independently from Greyhound.

Dependencies:
 - `PDAL`_ compiled with `LazPerf`_ compression enabled (``-DWITH_LAZPERF=ON``)
 - `Node.js`_ 10.29 or greater
 - C++11 compiler

.. _`PDAL`: http://www.pdal.io/index.html
.. _`Node.js`: http://nodejs.org/
.. _`LazPerf`: https://github.com/verma/laz-perf

Global NPM Dependencies
-------------------------------------------------------------------------------

NPM dependencies may be installed via the Node.js package manager "npm", which is included with an installation of Node.js.

NPM dependencies:
 - ``hipache``
 - ``node-gyp``
 - ``nodeunit`` (unit testing module - optional)

These packages may be installed after Node.js is installed with ``npm install -g hipache node-gyp nodeunit``.  The ``-g`` flag specifies global installation.

Obtaining, Building, and Installing
===============================================================================

Greyhound building and installation is accomplished via Makefile.

Minimum installation steps:
 - ``git clone https://github.com/hobu/greyhound.git && cd greyhound``
 - ``make``
 - ``cp config.template.js config.js`` - then configure this file with your port, path, and caching settings
 - ``make install``
 - ``greyhound auto`` - register Greyhound as an init.d service for Ubuntu (optional)
 - ``greyhound start``

Makefile targets
-------------------------------------------------------------------------------

Targets:
 - ``required`` - Install NPM dependencies for each Greyhound component and build the C++ session handler.  This is the default ``make`` target.
 - ``all`` - Perform ``make required`` and then build the C++ examples.
 - ``cpp`` - Build the C++ controller via ``node-gyp``.
 - ``npm`` - Install NPM dependencies for each Greyhound component as specified by the ``package.json`` file of each component.
 - ``clean`` - Clean executables from the session-handler and C++ examples.
 - ``install`` - Copy necessary Greyhound libraries to ``/var/greyhound/`` and install the ``greyhound`` utility command into ``/usr/bin/``.
 - ``install PROXY=OFF`` - Install without proxy, so that the HTTP/WS services are accessible over different public ports.
 - ``uninstall`` - Remove all traces of Greyhound installation (including log files).

Commanding Greyhound
-------------------------------------------------------------------------------

A utility command called ``greyhound`` is provided with the Greyhound installation.  This command provides simple access to some common Greyhound tasks.  Commands are of the format ``greyhound <COMMAND>``

Commands:
 - ``start`` - Launch all Greyhound ``init.d`` services (requires root).
 - ``stop`` - Stop all Greyhound ``init.d`` services (requires root).
 - ``status`` - Display running Greyhound services and each of their listening ports.
 - ``auto`` - An *Ubuntu-specific* command to register Greyhound services for auto-launch on boot.
 - ``rmauto`` - An *Ubuntu-specific* command to unregister Greyhound services from auto-launching.

Logging
-------------------------------------------------------------------------------

Greyhound logs are written to ``/var/log/greyhound/``.

Internal Configuration
===============================================================================

Configuration file
-------------------------------------------------------------------------------

After installation, Greyhound may be configured through a JavaScript configuration file located at ``/var/greyhound/config.js``.  This file specifies parameters for each individual Greyhound component, and the configuration is used by Greyhound at startup (so changes to this file require Greyhound to be relaunched).

Each component configuration allows a ``port`` value to be defined, on which the specified server component will listen.  If the frontend-proxy is used, then the ``port`` parameters specified in ``config.js`` do not need to be accessible via the outside world.  For more information on web server settings, and on public-facing port definitions, see `Front-end Proxy Settings`_.

Front-end Proxy Settings
-------------------------------------------------------------------------------

The *front-end proxy* allows connections over a single port (e.g. 80) to be used for both HTTP and WebSocket interfaces.

The proxy is configured via ``/var/greyhound/frontend-proxy/hipache-config.json``.  If used, the values for back-end ports much match those specified in ``config.js``, and the `frontend fe` binding is the only public-facing Greyhound port.

