===============================================================================
Greyhound - Client Development
===============================================================================

:author: Connor Manning
:email: connor@hobu.co
:date: 09/30/2016

Overview
===============================================================================

Greyhound is a dynamic point cloud server architecture that performs progressive level-of-detail streaming of indexed resources on-demand.  Greyhound can also stream unindexed files in a binary format specified by a client.

Using Greyhound
-------------------------------------------------------------------------------

Greyhound provides a simple HTTP interface to request information and data from remote point cloud resources.

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
| count         | Count points for a query without downloading them.          |
+---------------+-------------------------------------------------------------+
| hierarchy     | Get a metadata hierarchy with point counts information.     |
+---------------+-------------------------------------------------------------+
| files         | Get the metadata for files from the unindexed dataset.      |
+---------------+-------------------------------------------------------------+

|

Making Requests
-------------------------------------------------------------------------------

Greyhound's API is exposed via HTTP or HTTPS, using GET requests of the form ``http://<greyhound-server>/resource/<resource-name>/<command>?<options>``

The HTTP body of Greyhound's response contains the result of the request, which is either a JSON object for the ``info`` and ``hierarchy`` queries, or binary point data for the ``read`` query.  A response to ``read`` also contains some necessary information about the response as HTTP header data (see `The Read Query`_ for details).

|

The Info Query
===============================================================================

The `info` command returns a JSON structure with various metadata for the requested resource.  For the HTTP interface, this is contained in the body of the response - for WebSockets, this is contained in the response key ``info``.

The keys present in this JSON object are detailed below.

type
-------------------------------------------------------------------------------

*Type*: String

*Description*: Equal to "octree".  This key exists for future expansion.

numPoints
-------------------------------------------------------------------------------

*Type*: Integer

*Description*: Number of points present in the resource.

bounds
-------------------------------------------------------------------------------

*Type*: Array of doubles.

*Description*: An array of length 6 containing the maximum and minimum values for each of X, Y, and Z dimensions.  The format is ``[xMin, yMin, zMin, xMax, yMax, zMax]``.  This bounding box is a cube, and represents the bounds which are recursively bisected for the octree indexing.  Clients should use this bounds for dynamic level-of-detail loading.

boundsConforming
-------------------------------------------------------------------------------

*Type*: Array of doubles.

*Description*: An array of length 6 containing the maximum and minimum values for each of X, Y, and Z dimensions.  The format is ``[xMin, yMin, zMin, xMax, yMax, zMax]``.  This bounding box represents a bounds that tightly conforms to the points in the resource.

baseDepth
-------------------------------------------------------------------------------

*Type*: Integer

*Description*: The first octree depth which contains point data.  If this value is non-zero, then no points exist in the depth range ``[0, baseDepth)``.

srs
-------------------------------------------------------------------------------

*Type*: String

*Description*: A well-known-text (WKT) string representing the spatial reference system of this resource.


schema
-------------------------------------------------------------------------------

*Type*: Array of Objects.

*Description*: An array of dimension information representing the native schema of a resource.  Each dimension object contains entries for `name`, `type`, and `size`, as shown below.  The dimensions requested during a ``read`` request should be a subset of these dimensions, and their types may be altered to suit a given application (see `The Read Query`_ for details).  Known dimensions are described in the `PDAL dimension registry`_, however there may be other dimensions present in the schema (typically application or format-defined dimensions).

.. _`PDAL dimension registry`: http://www.pdal.io/dimensions.html

+---------------+--------------------------------------------------------------------------------+
| Field         | Value                                                                          |
+===============+================================================================================+
| ``"name"``    | Dimension name.                                                                |
+---------------+--------------------------------------------------------------------------------+
| ``"type"``    | Dimension type.  Possible values: ``"signed"``, ``"unsigned"``, ``"floating"`` |
+---------------+--------------------------------------------------------------------------------+
| ``"size"``    | Dimension size in bytes.  Possible values: ``"1"``, ``"2"``, ``"4"``, ``"8"``  |
+---------------+--------------------------------------------------------------------------------+

A ``schema`` looks something like: ::

    [
        { "name": "X",                  "type": "floating", "size": "8" },
        { "name": "Y",                  "type": "floating", "size": "8" },
        { "name": "Z",                  "type": "floating", "size": "8" },
        { "name": "Intensity",          "type": "unsigned", "size": "2" },
        { "name": "Red",                "type": "unsigned", "size": "2" },
        { "name": "Green",              "type": "unsigned", "size": "2" },
        { "name": "Blue",               "type": "unsigned", "size": "2" },
        { "name": "ReturnNumber",       "type": "unsigned", "size": "1" },
        { "name": "NumberOfReturns",    "type": "unsigned", "size": "1" },
        { "name": "PointId",            "type": "unsigned", "size": "4" },
        { "name": "OriginId",           "type": "unsigned", "size": "4" }
    ]


scale
-------------------------------------------------------------------------------

*Type*: A single double, or an array of three doubles.  This key is **optional**, and may not be present for absolutely positioned resources.

*Description*: This field gives insight into the physical storage of the dataset.  If present, it generally corresponds to the resolution of the source data.  If this value is a scalar, like ``0.01``, then uniform scaling is implied - equivalent to the array ``[0.01, 0.01, 0.01]``.  The bounds information from the ``info`` call does not have this scale pre-applied, so ``bounds`` and ``boundsConforming`` are always absolutely positioned.

offset
-------------------------------------------------------------------------------

*Type*: Array of doubles.  This key is **optional**, and may not be present for absolutely positioned resources.

*Description*: This field gives insight into the physical storage of the dataset.  If present, then the data-on-disk is written with this offset applied.  The bounds information from the ``info`` call does not have this offset pre-applied, so ``bounds`` and ``boundsConforming`` are always absolutely positioned.

|

The Read Query
===============================================================================

This query returns binary point data from a given resource.  Following the binary point data, 4 bytes that indicate the number of points in the response are appended.  These may be parsed as a 32-bit unsigned integer, transmitted in network byte order.  If the last 4 bytes are zero, then those 4 bytes shall be the only 4 bytes in the response.

Depth Options
-------------------------------------------------------------------------------

Depth options allow a client to query varying levels of detail for a resource on demand.  A *depth* corresponds to a tree depth in a quad- or octree.  These depths correspond to a traditional tree starting at depth zero, which contains a single point (the center-most point in the set bounds).  Depth one contains 4 points (one in each quadrant) for a quadtree or 8 for an octree.  Assuming the data exists, each of those points contains its 4 or 8 child points, and so forth.  Each depth has 4\ :sup:`depth` points for a quadtree or 8\ :sup:`depth` points for an octree.  Point do not necessarily start at depth zero (see `baseDepth`_ for more information).

Available options for depth selection are:

- ``depth``: Query a single depth of the tree.
- ``depthEnd``: Query depths up to, but **not** including, this depth.  If ``depthBegin`` is not specified, then this query selects from depth zero until ``depthEnd``.
- ``depthBegin``: Must be used with ``depthEnd``.  Queries run from ``depthBegin`` (inclusive) to ``depthEnd`` (non-inclusive).  A query containing ``depthBegin=6`` and ``depthEnd=7`` is identical to a query of ``depth=6``.

If no depth parameters are present in a query, then all depths are selected.  This is only recommended if the spatial extents begin queried (see `Bounds option`_) are very small.

Bounds option
-------------------------------------------------------------------------------

The ``bounds`` option allows a client to select only a portion of the entire dataset's bounds, as given by the ``bounds`` field from The **Info** Query.  If this field is omitted, the total dataset bounds are queried.

For a 3-dimensional query, the array may be of length 6, formatted as ``[xMin, yMin, zMin, xMax, yMax, zMax]``.  An array of length 4, formatted as ``[xMin, yMin, xMax, yMax]`` will query the entire Z-range of the dataset within the given XY bounds.

If omitted, then the entire resource bounds are selected.  This is only recommended if the depth range is very shallow.

Transformation Options
-------------------------------------------------------------------------------

Values for ``scale`` and/or ``offset`` may be supplied, which allows for the use of a transformed local coordinate system.  A common use would be requested scaled integer data centered around the origin.

- ``scale`` - Either a non-zero number or an array of numbers of length 3, formatted as ``[xScale, yScale, zScale]``.  If this value is a number, then that number will be used for all three scale values.
- ``offset`` - An array of 3 numbers, formatted as ``[xOffset, yOffset, zOffset]``.

If one or both of these values are present, then the ``bounds`` of the query must already be transformed with these values.  For example, let's say that the ``info`` of a resource contains a bounds of ``[500, 500, 500, 700, 700, 700]``, and the client wants to receive data in a local coordinate system centered around the origin with a scale factor of ``0.1``.  In this case, a request might look like:

``/resource/something/read?depth=8&bounds=[-1000,-1000,-1000,1000,1000,1000]&scale=0.01&offset=[600,600,600]``

If ``scale`` and ``offset`` values are passed, and they are exactly equivalent to those present in the ``info`` query, then this results in a no-op transformation on the server since the data is already in the desired local coordinate space under-the-hood.

Filters
-------------------------------------------------------------------------------

An arbitrary filtering structure may be passed along with a ``read`` request, which can be used to filter out points that do not meet some criteria.  The syntax of the filter tree is the same as MongoDB's `Query`_ and `Logical`_ operator syntax, using the dimensions from `schema`_ as the column criteria.

A filter tree might look like: ::

    filter={"$or":[
        {"Red":{"$gt":200}},
        {"Blue":{"$gt":120,"$lt":130}},
        {"Classification":{"$nin":[2,3]}}
    ]}

Data from original source files may be requested with the special ``Path`` pseudo-dimension (which does not appear in the `schema`_), which will be index-optimized: ::

    filter={"Path":"tile-845.laz"}

Selecting an input file by its ``OriginId`` dimension is also index-optimized: ::

    filter={"Origin": 5}

.. _`Query`: https://docs.mongodb.com/manual/reference/operator/query-comparison/
.. _`Logical`: https://docs.mongodb.com/manual/reference/operator/query-logical/


Other options
-------------------------------------------------------------------------------

- ``schema``: Formatted the same way as `schema`_.  This specifies the formatting of the binary data returned by Greyhound.  If any dimensions in the query result cannot be coerced into the specified type and size, an error occurs.  If any specified dimensions do not exist in the native schema, their positions will be zero-filled.  If this option is omitted, resulting data will be formatted in accordance with the native resource `schema`_.
- ``compress``: If true, the resulting stream will be compressed with `laz-perf`_.  The ``schema`` parameter, if provided, is respected by the compressed stream.  If omitted, data is returned uncompressed.

.. _`laz-perf`: http://github.com/hobu/laz-perf

|

The Count Query
===============================================================================

The format of this query matches the ``read`` query, and performs this query internally to the server without streaming any point data to the client.  The result is an accurate number of points for this query as well as the number of chunks used to perform this query: ::

    {
        "points": 19700,
        "chunks": 1
    }

The value of ``chunks`` doesn't have much meaning in absolute terms, but may be used to compare the server-side weight of queries in comparison to one another.  A "chunk" represents a server-side fetch of indexed point cloud data from the storage back-end for the requested resource.  Note that due to server caching, repeatedly queried chunks do not need to be fetched every time their data is accessed.

|

The Hierarchy Query
===============================================================================

This query returns point count information for a given bounding box and depth, and also recursively for incrementing depths and bisected bounding boxes.  This query is only supported for indexed datasets (see `type`).

Purpose and Usage
-------------------------------------------------------------------------------

The hierarchy query is used to build a client-side version of the structure of portions of the indexed tree in advance of querying actual data.  It is recommended that some base amount of data is loaded before this query, since it may take longer than a typical data query to complete.  A client should only query the hierarchy for a few depths at a time, and then query ever-bisected sub-bounds for each subsequent depth range (for example, depths ``[8, 12)`` with the full bounds, but the bounds for queries of ``[12, 16)``, should be bisected 4 times from the full bounds).

Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The hierarchy query accepts options that are similar to those from the ``read`` query.

- ``bounds``: The overall bounds to query.
- ``scale``: Scale factor pre-applied to the requested ``bounds``.
- ``offset``: Offset pre-applied to the requested ``bounds``.
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

The Files Query
===============================================================================

Description
-------------------------------------------------------------------------------

This query returns a JSON structure containing the original metadata found in the selected input files which make up an indexed dataset.  The metadata for a single file may be returned as an object, or if multiple files are selected, an array of objects.  Files may be selected by their filename, their location in the index, or by selecting those files which overlap a queried bounds.

Single-selection form
-------------------------------------------------------------------------------

One form of the ``files`` query selects the metadata for a single file from the input of an index.  This selection is accomplished via a string portion of a file-path or a number specifying a unique sequence-location within the output index.

The file-path selection looks like ``/files/my-input-tile-42.laz``.  In this case, a substring match will be performed against the string ``my-input-tile-42.laz``.  The input to the ``files`` query must be specific enough to select only the file desired.  For example, if a dataset were comprised of files ``abc-1.laz`` and ``abc-2.laz``, a query of ``/files/abc`` could return either of those files.

A sequence-location selection looks like ``/files/2718``, which would select the file with an ``OriginId`` of ``2718``.  For each input file, every point from that file is assigned a unique ``OriginId`` dimension, so this query may be derived from the point data for a point received from a ``read`` query.

If no match can be made for either a partial path or an ``OriginId`` value, then ``null`` will be returned.

Query form
-------------------------------------------------------------------------------

The single-selection queries above may be alternatively made with a more flexible query-parameter format using the query parameter ``search``.  For the above examples, the corresponding queries would be ``/files?search=my-input-tile-42.laz`` and ``files?search=2718``.

In addition to the single-selection form, this form may also accept an array containing multiple queries similar to the above.  Their types may be mixed, for example the array may contain both file paths as well as ``OriginId`` values.  For example, ``/files?search=["my-input-tile-42.laz", 2718]``.  The array must be valid JSON, so strings must be quoted.  For each entry in the input query, the output will contain one entry, which may be null if no matches were found for an input entry.  If the query is of size one, then the result will be an object (or null if there are no results).

Bounds-overlap query
-------------------------------------------------------------------------------

File metadata may also be selected by querying for a ``bounds``, in which case all overlapping files from the input will be selected.  This query looks like ``/files?bounds=[100, 200, 300, 1100, 1200, 1300]`` or ``/files?bounds=[100, 200, 1100, 1200]``.  The format is ``[x-min, y-min, z-min, x-max, y-max, z-max]`` or ``[x-min, y-min, x-max, y-max]``.  Again, if only one match is found, then the result will be an object - if multiple matches are found, an array - and if zero, null.

Results
-------------------------------------------------------------------------------

A single result may look something like this, edited for brevity ::

    {
        "bounds": [635648.8200000001, 851234.24, 411.09, 636357.77, 852448.12, 556.69],
        "metadata": {
            "comp_spatialreference": "...",
            "compressed": true,
            "count": 591852,
            "creation_doy": 271,
            "creation_year": 2016,
            "dataformat_id": 3,
            "dataoffset": 1812,
            "filesource_id": 0,
            "global_encoding": 0,
            "global_encoding_base64": "AAA=",
            "header_size": 227,
            "major_version": 1,
            "maxx": 636357.77,
            "maxy": 852448.12,
            "maxz": 556.69,
            "minor_version": 2,
            "minx": 635648.82,
            "miny": 851234.24,
            "minz": 411.09,
            "offset_x": 0,
            "offset_y": 0,
            "offset_z": 0,
            "project_id": "00000000-0000-0000-0000-000000000000",
            "scale_x": 0.01,
            "scale_y": 0.01,
            "scale_z": 0.01,
            "software_id": "PDAL 1.3.0 (8a481e)",
            "spatialreference": "...",
            "srs": {
                "compoundwkt": "...",
                "horizontal": "...",
                "isgeocentric": false,
                "isgeographic": false,
                "prettycompoundwkt": "...",
                "prettywkt": "...",
                "proj4": "+proj=lcc +lat_1=43 +lat_2=45.5 +lat_0=41.75 +lon_0=-120.5 +x_0=399999.9999999999 +y_0=0 +ellps=GRS80 +units=ft +no_defs",
                "units": { "horizontal": "foot", "vertical": "" },
                "vertical": "",
                "wkt": "...",
            },
            "system_id": "PDAL",
            "vlr_0": { "description": "http://laszip.org", "record_id": 22204, "user_id": "laszip encoded" },
            "vlr_1": { ... },
            "vlr_2": { ... }
        },
        "numPoints": 591852,
        "origin": 1,
        "path": "my-data/tile-10.laz",
        "pointStats": { "inserts": 591852, "outOfBounds": 0, "overflows": 0 },
        "srs": "...",
        "status": "inserted"
    }

The ``metadata`` key is header metadata picked up by PDAL, so its contents are simply forwarded as-is.  Other keys are specific to Entwine's indexing.

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
