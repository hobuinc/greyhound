===============================================================================
Greyhound - Client Development
===============================================================================

:author: Connor Manning
:email: connor@hobu.co
:date: 08/17/2015

Overview
===============================================================================

Greyhound is a dynamic point cloud server architecture that performs progressive level-of-detail streaming of indexed resources on-demand.  Greyhound can also stream unindexed files in a binary format specified by a client.

Using Greyhound
-------------------------------------------------------------------------------

Greyhound's provides a simple HTTP interface to request information and data from remote point cloud resources.

.. _`WebSocket`: http://en.wikipedia.org/wiki/WebSocket

|

API
===============================================================================

Command Set
-------------------------------------------------------------------------------

+---------------+-------------------------------------------------------------+
| Command       | Function                                                    |
+===============+=============================================================+
| info          | Get the JSON metadata for a resource.                       |
+---------------+-------------------------------------------------------------+
| read          | Read points from a resource.                                |
+---------------+-------------------------------------------------------------+
| hierarchy     | Get a metadata hierarchy with point counts information.     |
+---------------+-------------------------------------------------------------+

|

Making Requests
-------------------------------------------------------------------------------

HTTP Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Greyhound's primary interface is over HTTP, using GET requests of the form ``http://<greyhound-server>/resource/<resource-name>/<command>?<options>``

The HTTP body of Greyhound's response contains the result of the request, which is either a JSON object for the ``info`` query, or binary point data for the ``read`` query.  A response to ``read`` also contains some necessary information about the response as HTTP header data (see `The Read Query`_ for details).

WebSocket Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Greyhound communicates via JSON objects for its WebSocket interface.  Requests are of the form:

+-----------------------------------------------------------------------------+
| Command                                                                     |
+---------------------+-------------+-----------------------------------------+
| Key                 | Type        | Value                                   |
+=====================+=============+=========================================+
| ``"command"``       | String      | Command name - `"info"` or `"read"`.    |
+---------------------+-------------+-----------------------------------------+
| (command parameters)| Various     | Parameters for `"read"` command.        |
+---------------------+-------------+-----------------------------------------+

Responses from Greyhound look like:

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-----------------------+--------------+--------------------------------------------------+
| Key                   | Type         | Value                                            |
+=======================+==============+==================================================+
| ``"command"``         | String       | Command name received in initial command.        |
+-----------------------+--------------+--------------------------------------------------+
| ``"status"``          | Integer      | ``1`` for success, else ``0``.                   |
+-----------------------+--------------+--------------------------------------------------+
| (``"message"``)       | String       | If success if ``0``, this may contain a reason.  |
+-----------------------+--------------+--------------------------------------------------+
| (response parameters) | Various      | Command-dependent values                         |
+-----------------------+--------------+--------------------------------------------------+

See the details for these commands for more information about command-dependent parameters and values.

|

The Info Query
===============================================================================

The `info` command returns a JSON structure with various metadata for the requested resource.  For the HTTP interface, this is contained in the body of the response - for WebSockets, this is contained in the response key ``info``.

The keys present in this JSON object are detailed below.

type
-------------------------------------------------------------------------------

*Type*: String

*Description*: Equal to "unindexed", "quadtree", or "octree".  Resources fall into two classes, indexed and unindexed, and the rest of this document will refer to resources of type "octree" or "quadtree" as *indexed* resources.

numPoints
-------------------------------------------------------------------------------

*Type*: Integer

*Description*: Number of points present in the resource.

bounds
-------------------------------------------------------------------------------

*Type*: Array of doubles.

*Description*: An array of length 6 containing the maximum and minimum values for each of X, Y, and Z dimensions.  The format is ``[xMin, yMin, zMin, xMax, yMax, zMax]``.

schema
-------------------------------------------------------------------------------

*Type*: Array of Objects.

*Description*: An array of dimension information representing the native schema of a resource.  Each dimension object contains entries for `name`, `type`, and `size`, as shown below.  The dimensions requested during a ``read`` request should be a subset of these dimensions, and their types may be altered to suit a given application (see `The Read Query`_ for details).

+---------------+--------------------------------------------------------------------------------+
| Field         | Value                                                                          |
+===============+================================================================================+
| ``"name"``    | Dimension name.                                                                |
+---------------+--------------------------------------------------------------------------------+
| ``"type"``    | Dimension type.  Possible values: ``"signed"``, ``"unsigned"``, ``"floating"`` |
+---------------+--------------------------------------------------------------------------------+
| ``"size"``    | Dimension size in bytes.  Possible values: ``"1"``, ``"2"``, ``"4"``, ``"8"``  |
+---------------+--------------------------------------------------------------------------------+

An ``schema`` looks something like: ::

    [
        {
            "name": "X",
            "type": "floating",
            "size": "8"
        },
        {
            "name": "Y",
            "type": "floating",
            "size": "8"
        },
        {
            "name": "Z",
            "type": "floating",
            "size": "8"
        },
        {
            "name": "Intensity",
            "type": "unsigned",
            "size": "2"
        },
        {
            "name": "Red",
            "type": "unsigned",
            "size": "2"
        },
        {
            "name": "Green",
            "type": "unsigned",
            "size": "2"
        },
        {
            "name": "Blue",
            "type": "unsigned",
            "size": "2"
        },
        {
            "name": "ReturnNumber",
            "type": "unsigned",
            "size": "1"
        },
        {
            "name": "NumberOfReturns",
            "type": "unsigned",
            "size": "1"
        },
        {
            "name": "Origin",
            "type": "unsigned",
            "size": "4"
        }
    ]

|

The Read Query
===============================================================================

This query returns binary point data from a given resource.  Following the binary point data, 4 bytes that indicate the number of points in the response are appended.  These may be parsed as a 32-bit unsigned integer, transmitted in network byte order.  If the last 4 bytes are zero, then those 4 bytes shall be the only 4 bytes in the response.

Unindexed
-------------------------------------------------------------------------------

For unindexed resources (see `type`_), the only supported *read* query is a query for all available points in the resource.  Only `Read Options - Common`_ are supported.

Indexed
-------------------------------------------------------------------------------

For indexed resources, in addition to the common options, queries for tree depths and bounds subsets are supported.  This allows a client to progressively load points at higher levels of detail only where such detail is warranted.

Depth Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Depth options allow a client to query varying levels of detail for a resource on demand.  A *depth* corresponds to a tree depth in a quad- or octree.  These depths correspond to a traditional tree starting at depth zero, which contains a single point (the center-most point in the set bounds).  Depth one contains 4 points (one in each quadrant) for a quadtree or 8 for an octree.  Assuming the data exists, each of those points contains its 4 or 8 child points, and so forth.  Each depth has 4\ :sup:`depth` points for a quadtree or 8\ :sup:`depth` points for an octree.

Available options for depth selection are:

- ``depth``: Query a single depth of the tree.
- ``depthEnd``: Query depths up to, but **not** including, this depth.  If ``depthBegin`` is not specified, then this query selects from depth zero until ``depthEnd``.
- ``depthBegin``: Must be used with ``depthEnd``.  Queries run from ``depthBegin`` (inclusive) to ``depthEnd`` (non-inclusive).  A query containing ``depthBegin=6`` and ``depthEnd=7`` is identical to a query of ``depth=6``.

Bounds option
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``bounds`` option allows a client to select only a portion of the entire dataset's bounds, as given by the ``bounds`` field from The **Info** Query.  If this field is omitted, the total dataset bounds are queried.

For a 3-dimensional query, the array may be of length 6, formatted as ``[xMin, yMin, zMin, xMax, yMax, zMax]``.  An array of length 4, formatted as ``[xMin, yMin, xMax, yMax]`` will query the entire Z-range of the dataset within the given XY bounds.

Read Options - Common
-------------------------------------------------------------------------------

Common options are options available for any ``read`` query, regardless of the ``type`` of resource.

- ``schema``: Formatted the same way as `schema`_.  This specifies the formatting of the binary data returned by Greyhound.  If any dimensions in the query result cannot be coerced into the specified type and size, an error occurs.  If any specified dimensions do not exist in the native schema, their positions will be zero-filled.  If this option is omitted, resulting data will be formatted in accordance with the native resource `schema`_.
- ``compress``: If true, the resulting stream will be compressed with `laz-perf`_.  The ``schema`` parameter, if provided, is respected by the compressed stream.  If omitted, data is returned uncompressed.

.. _`laz-perf`: http://github.com/verma/laz-perf

|

The Hierarchy Query
===============================================================================

This query returns point count information for a given bounding box and depth, and also recursively for incrementing depths and bisected bounding boxes.  This query is only supported for indexed datasets (see `type`).

Purpose and Usage
-------------------------------------------------------------------------------

The hierarchy query is used to build a client-side version of the structure of portions of the indexed tree in advance of querying actual data.  It is recommended that some base amount of data is loaded before this query, since it may take longer than a typical data query to complete.  A client should only query the hierarchy for a few depths at a time, and then query ever-bisected sub-bounds for each subsequent depth range (for example, depths ``[8, 12)`` with the full bounds, but the bounds for queries of ``[12, 16)``, should be bisected 4 times from the full bounds).

Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The hierarchy query accepts exactly three options, which are similar to those for the ``read`` query.

- ``bounds``: The overall bounds to query.
- ``depthBegin``: The starting depth to begin the query for the full specified ``bounds``.
- ``depthEnd``: Similar to the ``read`` query, queries run from ``depthBegin`` (inclusive) to ``depthEnd`` (non-inclusive).

Returned data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The hierarchy query returns JSON data, which at the top level, contains the number of points at depth ``depthBegin`` within the full ``bounds`` box.  Point counts are specified with the ``n`` JSON key.  Nested within the top-level JSON response are subsequent levels up to ``depthEnd``, where each new nesting level represents another level of the recursively bisected ``bounds``.

Bisection directions are denoted by 8 keys for octrees (4 for quadtrees) representing the direction of the split in the native point space.  In this space, we consider North to be an increase in Y (with decrease being South), East to mean an increase in X (with decrease being West), and Up to be an increase in Z (decrease being Down).  The first letter of each of these directions is concatenated in the previously mentioned order, which is more simply shown with an example:

+-----------+-----------------+
| Key       | Meaning         |
+===========+=================+
| ``"nwu"`` | North-west-up   |
+-----------+-----------------+
| ``"nwd"`` | North-west-down |
+-----------+-----------------+
| ``"neu"`` | North-east-up   |
+-----------+-----------------+
| ``"ned"`` | North-east-down |
+-----------+-----------------+
| ``"swu"`` | South-west-up   |
+-----------+-----------------+
| ``"swd"`` | South-west-down |
+-----------+-----------------+
| ``"seu"`` | South-east-up   |
+-----------+-----------------+
| ``"sed"`` | South-east-down |
+-----------+-----------------+

For quadtree queries, the third character is omitted, so possible keys are ``nw``, ``ne``, ``sw``, and ``se``.

Within each tree depth of the response, the number of points indicated by a traversal is indicated with the key ``n``.  A sample response for a call of ``/hierarchy?bounds=[0, 0, 0, 1000, 1000, 1000]&depthBegin=8&depthEnd=11`` might look like: ::

    {
        "n": 158192,
        "ned": {
            "n": 138599,
            "neu": {
                "n": 130674
            },
            "nwu": {
                "n": 98252
            },
            "seu": {
                "n": 127565
            },
            "swu": {
                "n": 81784
            }
        },
        "neu": {
            "n": 13653,
            "ned": {
                "n": 12531
            },
            "sed": {
                "n": 18163
            },
            "swd": {
                "n": 4617
            }
        },
        ... // etc.
    }

This result indicates that at depth 8, for the entire queried bounds, there are 158192 points.

At depth 9, for the north-east-down (``ned``) bisected bounds, which would be ``[500, 500, 0, 1000, 1000, 500]``, there are 138599 points.  For ``neu`` at depth 9, being ``[500, 500, 500, 1000, 1000, 1000]``, there are 13653 points.

At depth 10, starting from the ``ned`` bounds, the ``neu`` bounds of ``[750, 750, 250, 1000, 1000, 500]`` contains 13064 points.  Since there is no key for ``["ned"]["ned"]``, there are zero points at depth 10 for bounds ``[750, 750, 0, 1000, 1000, 250]``.

|

Working with Greyhound
===============================================================================

Errors
-------------------------------------------------------------------------------

Greyhound errors result in standard HTTP error codes.  Invalid options or improper formatting will result in a ``400 - client error``, meaning the request should not be repeated without modification.  If the query is valid but cannot be process, a status code of ``500 - internal server error`` will be returned.

For indexed datasets, a query that is too large will result in a ``413 - entity too large`` error code.  This means that the query requires fetches of too many remotely stored chunks of data, so Greyhound refuses to process it.  The exact maximum count depends both on how the data was indexed and how the server was configured, so a client should be prepared to react to this error code by either shrinking the requested bounds or lowering the requested depth.  This allows Greyhound to maintain fast response times for all users and urges clients to develop a query pattern that results quick feedback to the user during progressive loading.

Optimizing Server Performance
-------------------------------------------------------------------------------

A client's query pattern can significantly affect their performance, even while staying under the ``413`` limits imposed by the server.  Some basic tips for query patterns follow.

Initial Fetch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A client should always start by requesting the ``info`` for a given resource, and store the entire result.

This allows a client to avoid querying non-existent dimensions, for example a web renderer that generally queries Red, Green, and Blue dimensions should not do so if those dimensions do not exist in the native schema.

Progressive Querying
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For indexed datasets, a client should start with a single conservative "base" request - requesting depths zero until some fixed depth, rather than making small requests starting at depth zero.  If the response is a ``413``, the client can continually lower the initial depth until a valid response is received.  The exact depth depends on the application, but this request has a well-defined maximum number of points - for example an octree query with ``depthBegin=0`` and ``depthEnd=8`` will result in 2396745 points at a maximum (8\ :sup:`0` + 8\ :sup:`1` + ... + 8\ :sup:`7` = 2396745).

The "base" query is a request that gives quick feedback to a user of the entire set at a low resolution.  After this is displayed, a client should start splitting their ``bounds`` in the request as they move upward in depth.  In general, a query of depth ``n + 1`` should have one-fourth the volume of depth ``n`` for quadtrees, or one-eight for octrees.  So for example, if the base depth query is 8, a client may decide to issue 8 queries of ``depth=8``, one for each octant of the overall bounds.  For each query whose result contains a non-zero number of points, that octant may be again split into its 8 octants, and the process repeats.  This pattern allows the client to prune their search space - if a query of a given bounds returns zero points at depth ``n``, then there are also zero points for those bounds at depth ``n + 1``.

The exact depths and number of splits (for example, the base depth of 8 could have been split into 64 queries if the client wanted faster pruning of the cuboids) depends on the application and should be found via experimentation.  Too small of queries will prune the search space quickly, but will result in many queries with few points.  Too large of queries can result in a ``413`` and will fail to prune the search space effectively.

Sample Queries
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section shows some full HTTP requests for various queries, assuming a Greyhound server is running on localhost with an octree resource named `the-moon`.

- Get the metadata info: ``localhost/resource/the-moon/info``

- Query compressed data up to depth 8, fetching only X, Y, Z, and Intensity for the entire dataset bounds - where X, Y, and Z are requested as 4-byte floats and Intensity is a 2-byte unsigned integer: ``localhost/resource/the-moon/read?depthEnd=8&schema=[{"name":"X","type":"floating","size":"4"},{"name":"Y","type":"floating","size":"4"},{"name":"Z","type":"floating","size":"4"},{"name":"Intensity","type":"unsigned","size":"2"}]&compress=true``

- Query uncompressed data at depth 12 within a given bounds, fetching XYZRGB values as single-byte unsigned integers: ``localhost/resource/the-moon/read?depth=12&bounds=[275,100,25,287.5,112.5,50]&schema=[{"name":"X","type":"floating","size":"4"},{"name":"Y","type":"floating","size":"4"},{"name":"Z","type":"floating","size":"4"},{"name":"Red","type":"unsigned","size":"1"},{"name":"Green","type":"unsigned","size":"1"},{"name":"Blue","type":"unsigned","size":"1"}]``
