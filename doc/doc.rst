===============================================================================
Greyhound
===============================================================================

:author: Connor Manning
:email: connor@hobu.com
:date: 09/30/2014

Overview
===============================================================================

Greyhound is a distributed server architecture that abstracts and maintains persistent sessions for PDAL pipelines used for real-time querying of point cloud data.

Using Greyhound
===============================================================================

Connecting
-------------------------------------------------------------------------------

TODO

Issuing Commands
-------------------------------------------------------------------------------

TODO

Disconnecting
-------------------------------------------------------------------------------

TODO

API
===============================================================================

Command Set
-------------------------------------------------------------------------------

+---------------+-------------------------------------------------------------+
| Command       | Function                                                    |
+===============+=============================================================+
| put           | Store a pipeline.                                           |
+---------------+-------------------------------------------------------------+
| create        | Create a PDAL session using a previously stored pipeline.   |
+---------------+-------------------------------------------------------------+
| pointsCount   | Get the number of points present in a session.              |
+---------------+-------------------------------------------------------------+
| schema        | Get the Greyhound schema of a session.                      |
+---------------+-------------------------------------------------------------+
| srs           | Get the spatial reference system of a session.              |
+---------------+-------------------------------------------------------------+
| read          | Read points from a session.                                 |
+---------------+-------------------------------------------------------------+
| cancel        | Cancel an ongoing 'read' command.                           |
+---------------+-------------------------------------------------------------+
| destroy       | Destroy an active session.                                  |
+---------------+-------------------------------------------------------------+

|

Command and Response format
-------------------------------------------------------------------------------

Greyhound commands are issued as JSON objects over a WebSocket connection, and every command sent to Greyhound will initiate a JSON response.  Strings are UTF-8 encoded, and all commands are case sensitive.

Parentheses ``()`` around a key indicate optional or conditional keys that will not necessarily be present.  Quotation marks ``""`` around a field indicate a string literal, as opposed to a description.

Each command shares a common basic format:

+------------------------------------------------------------------------------------+
| Command                                                                            |
+---------------------+-------------+------------------------------------------------+
| Key                 | Type        | Value                                          |
+=====================+=============+================================================+
| ``"command"``       | String      | Command name                                   |
+---------------------+-------------+------------------------------------------------+
| (command parameters)| Various     | Command-dependent values                       |
+---------------------+-------------+------------------------------------------------+

|

Greyhound only transmits data in response to a command, and does not send any transmissions unprompted.  Responses also share a common format:

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-----------------------+--------------+--------------------------------------------------+
| Key                   | Type         | Value                                            |
+=======================+==============+==================================================+
| ``"command"``         | String       | Command name received in initial command         |
+-----------------------+--------------+--------------------------------------------------+
| ``"status"``          | Integer      | ``1`` for success, else ``0``                    |
+-----------------------+--------------+--------------------------------------------------+
| (``"message"``)       | String       | Information message regarding this command       |
+-----------------------+--------------+--------------------------------------------------+
| (``"reason"``)        | String       | Notification of a problem with this command      |
+-----------------------+--------------+--------------------------------------------------+
| (response parameters) | Various      | Command-dependent values                         |
+-----------------------+--------------+--------------------------------------------------+

|

If the returning ``status`` field is ``0``, then ``reason`` will contain an error message if applicable.  If the returning ``status`` is zero, then there are no valid command-dependent response parameters.

There is only one exception to this command-and-response format, which occurs only for the ``read`` command, which includes a binary transmission following the traditional JSON response.  This scenario is the only non-JSON output from Greyhound.  See `Read (basics)`_ for details.

Command Details
-------------------------------------------------------------------------------

----

Put
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-------------------------------------------------------------------------------------+
| Command                                                                             |
+-------------------+------------+----------------------------------------------------+
| Key               | Type       | Value                                              |
+===================+============+====================================================+
| ``"command"``     | String     | ``"put"``                                          |
+-------------------+------------+----------------------------------------------------+
| ``"pipeline"``    | String     | PDAL pipeline XML                                  |
+-------------------+------------+----------------------------------------------------+

+-------------------------------------------------------------------------------------+
| Response                                                                            |
+-------------------+------------+----------------------------------------------------+
| Key               | Type       | Value                                              |
+===================+============+====================================================+
| ``"command"``     | String     | ``"put"``                                          |
+-------------------+------------+----------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                      |
+-------------------+------------+----------------------------------------------------+
| ``"pipelineId"``  | String     | Greyhound pipeline ID                              |
+-------------------+------------+----------------------------------------------------+

Notes:
 - ``pipeline``: must contain valid PDAL XML, which will be validated before storage.  If the pipeline XML is not valid, the returning ``status`` will be ``0`` and the pipeline will not be stored.
 - ``pipelineId``: used in the future to instantiate a PDAL session for this pipeline.  A given pipeline XML string will always return the same ``pipelineId`` value.

----

Create
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-------------------------------------------------------------------------------+
| Command                                                                       |
+-----------------+------------+------------------------------------------------+
| Key             | Type       | Value                                          |
+=================+============+================================================+
| ``"command"``   | String     | ``"create"``                                   |
+-----------------+------------+------------------------------------------------+
| ``"pipelineId"``| String     | Greyhound pipeline ID                          |
+-----------------+------------+------------------------------------------------+

+-------------------------------------------------------------------------------------+
| Response                                                                            |
+-------------------+------------+----------------------------------------------------+
| Key               | Type       | Value                                              |
+===================+============+====================================================+
| ``"command"``     | String     | ``"create"``                                       |
+-------------------+------------+----------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                      |
+-------------------+------------+----------------------------------------------------+
| ``"session"``     | String     | Greyhound session ID                               |
+-------------------+------------+----------------------------------------------------+

Notes:
 - ``pipelineId``: stored from the results of a previous ``put`` command.  If the given ``pipelineId`` does not exist within Greyhound, then the returning ``status`` will be ``0``.
 - ``session``: represents a token required for future use of this session.  All Greyhound commands except for ``put`` and ``create`` require an active Greyhound session token as a parameter.

----

Points Count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------------+
| Command                                                                     |
+---------------+------------+------------------------------------------------+
| Key           | Type       | Value                                          |
+===============+============+================================================+
| ``"command"`` | String     | ``"pointsCount"``                              |
+---------------+------------+------------------------------------------------+
| ``"session"`` | String     | Greyhound session ID                           |
+---------------+------------+------------------------------------------------+

+-------------------------------------------------------------------------------------+
| Response                                                                            |
+-------------------+------------+----------------------------------------------------+
| Key               | Type       | Value                                              |
+===================+============+====================================================+
| ``"command"``     | String     | ``"pointsCount"``                                  |
+-------------------+------------+----------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                      |
+-------------------+------------+----------------------------------------------------+
| ``"count"``       | Integer    | Number of points in this session                   |
+-------------------+------------+----------------------------------------------------+

----

Schema
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------------+
| Command                                                                     |
+---------------+------------+------------------------------------------------+
| Key           | Type       | Value                                          |
+===============+============+================================================+
| ``"command"`` | String     | ``"schema"``                                   |
+---------------+------------+------------------------------------------------+
| ``"session"`` | String     | Greyhound session ID                           |
+---------------+------------+------------------------------------------------+

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"command"``     | String     | ``"pointsCount"``                                      |
+-------------------+------------+--------------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                          |
+-------------------+------------+--------------------------------------------------------+
| ``"schema"``      | String     | JSON stringified Greyhound schema for this session     |
+-------------------+------------+--------------------------------------------------------+

Notes:
 - ``schema``: see `Session Schema`_.

----

Spatial Reference System
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------------+
| Command                                                                     |
+---------------+------------+------------------------------------------------+
| Key           | Type       | Value                                          |
+===============+============+================================================+
| ``"command"`` | String     | ``"srs"``                                      |
+---------------+------------+------------------------------------------------+
| ``"session"`` | String     | Greyhound session ID                           |
+---------------+------------+------------------------------------------------+

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"command"``     | String     | ``"srs"``                                              |
+-------------------+------------+--------------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                          |
+-------------------+------------+--------------------------------------------------------+
| ``"srs"``         | String     | Spatial reference system for this session              |
+-------------------+------------+--------------------------------------------------------+

Notes:
 - ``srs``: a string formatted by PDAL representing the spatial reference system.

----

Read (Basics)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------------------------------------------------------------------------+
| Command                                                                                |
+---------------------+------------+-----------------------------------------------------+
| Key                 | Type       | Value                                               |
+=====================+============+=====================================================+
| ``"command"``       | String     | ``"read"``                                          |
+---------------------+------------+-----------------------------------------------------+
| ``"session"``       | String     | Greyhound session ID                                |
+---------------------+------------+-----------------------------------------------------+
| (``"schema"``)      | String     | JSON stringified schema for return data             |
+---------------------|------------+-----------------------------------------------------+

Notes:
 - ``schema``: If omitted, ``read`` results will be formatted as the schema returned from `Schema`_.  Client may optionally supply a different schema format for the results of this ``read``.  See `Manipulating the Schema`_.

|

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"command"``     | String     | ``"read"``                                             |
+-------------------+------------+--------------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                          |
+-------------------+------------+--------------------------------------------------------+
| ``"readId"``      | String     | Identification token for this ``read`` request         |
+-------------------+------------+--------------------------------------------------------+
| ``"numPoints"``   | Integer    | Number of points that will be transmitted - may be zero|
+-------------------+------------+--------------------------------------------------------+
| ``"numBytes"``    | Integer    | Number of bytes that will be transmitted - may be zero |
+-------------------+------------+--------------------------------------------------------+


Notes:
 - ``readId``: This identification string is required to cancel this ``read`` request (see `Cancel`_).
 - ``numPoints``: Number of points that will follow in a binary transmission.
 - ``numBytes``: Number of bytes that will follow in a binary transmission.

After Greyhound transmits the above JSON response, if ``numBytes`` is non-zero, a binary transmission sequence will follow.  This binary data will arrive in the format specified by ``schema`` (see `Schema`_) if one is supplied as a parameter to ``read``, or as the default returned by the ``schema`` query.

If ``numBytes`` is non-zero (and ``status`` is ``1``), a client should expect to consume ``numBytes`` bytes of binary data.  After ``numBytes`` of binary data is has arrived, the ``read`` response is complete.

|

Important:
 - Because binary data from multiple ``read`` commands cannot be differentiated, no new ``read`` command should be issued over a single websocket connection until a previous ``read`` query completes or is successfully cancelled.  All other commands may still be issued during this time period.
 - There is no further response from Greyhound to indicate that a ``read`` transmission is complete, so a client must take note of ``numBytes`` and track the number of binary bytes received accordingly.
 - Binary data may arrive in multiple "chunked" transmissions.  Chunk size may vary, even within the same response sequence.  Chunks will always arrive in order and may be appended.  Chunk boundaries may not align with point or dimension boundaries, so a single point, or even a single dimension within a point, may be spread across multiple chunks.

----

Read (Raster Basics)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"command"``     | String     | ``"read"``                                             |
+-------------------+------------+--------------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                          |
+-------------------+------------+--------------------------------------------------------+
| ``"readId"``      | String     | Identification token for this ``read`` request         |
+-------------------+------------+--------------------------------------------------------+
| ``"numPoints"``   | Integer    | Number of points that will be transmitted - may be zero|
+-------------------+------------+--------------------------------------------------------+
| ``"numBytes"``    | Integer    | Number of bytes that will be transmitted - may be zero |
+-------------------+------------+--------------------------------------------------------+
| ``"rasterMeta"``  | Object     | Raster dimensional metadata                            |
+-------------------+------------+--------------------------------------------------------+

Notes:
 - The initial response is the same as the response for non-rasterized queries, with the addition of the ``rasterMeta`` JSON object.  The binary data is formatted differently from non-rasterized ``read`` queries (see below).
 - If a ``schema`` parameter is included in the rastered ``read`` command, then it must contain ``X``, ``Y``, and at least one other dimension.

|

``rasterMeta`` contains information about the dimensions of the raster:

+-----------------------------------------------------------------------------------------+
| ``rasterMeta``                                                                          |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"xMin"``        | Float      | Lower X bound                                          |
+-------------------+------------+--------------------------------------------------------+
| ``"xStep"``       | Float      | X value difference between adjacent coordinate entries |
+-------------------+------------+--------------------------------------------------------+
| ``"xNum"``        | Integer    | Number of steps in the X direction                     |
+-------------------+------------+--------------------------------------------------------+
| ``"yMin"``        | Float      | Lower Y bound                                          |
+-------------------+------------+--------------------------------------------------------+
| ``"yStep"``       | Float      | Y value difference between adjacent coordinate entries |
+-------------------+------------+--------------------------------------------------------+
| ``"yNum"``        | Integer    | Number of steps in the Y direction                     |
+-------------------+------------+--------------------------------------------------------+

The format of the binary transmission following the initial response follows the information in ``rasterMeta``.  Starting at offset ``0``, the first bytes of the binary data represent coordinate ``(xMin, yMin)``.  At offset ``0 + <reduced schema size>``, where ``reduced schema size`` is the schema size excluding ``X`` and ``Y`` values, the coordinate represented is ``(xMin + xStep, yMin)``.  After an offset of ``xNum * <reduced schema size>``, the represented ``Y`` coordinate increments by ``yStep``.  See `Raster Metadata`_ for further information.

Important:
 - Each point in the rasterized binary format does not explicitly contain ``X`` and ``Y`` dimension values.  These values are implicit from the information in ``rasterMeta``.
 - Therefore the size of each point in the binary schema does not include the sizes of ``X`` or ``Y``.  In the ``schema`` parameter sent with the ``read`` command, the ``size`` and ``type`` of these dimensions may be omitted, and will be ignored if included.

----

Read - Unindexed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------------------------------------------------------------------------+
| Command                                                                                |
+---------------------+------------+-----------------------------------------------------+
| Key                 | Type       | Value                                               |
+=====================+============+=====================================================+
| ``"command"``       | String     | ``"read"``                                          |
+---------------------+------------+-----------------------------------------------------+
| ``"session"``       | String     | Greyhound session ID                                |
+---------------------+------------+-----------------------------------------------------+
| (``"schema"``)      | String     | JSON stringified schema for return data             |
+---------------------+------------+-----------------------------------------------------+
| (``"start"``)       | Integer    | Starting offset from which to read                  |
+---------------------+------------+-----------------------------------------------------+
| (``"count"``)       | Integer    | Number of points to read sequentially from ``start``|
+---------------------+------------+-----------------------------------------------------+

Notes:
 - See `Read (Basics)`_ for information on the Greyhound response.
 - ``start``: If omitted or negative, defaults to zero.  If greater than or equal to the value returned by `Points Count`_, no points will be read.
 - ``count``: If omitted or negative, reads from ``start`` through the last point.  If the sum of ``start`` and ``count`` is greater than or equal to the value returned by `Points Count`_, the ``read`` will read from ``start`` through the last point.
 - A client that simply wants to duplicate the entire buffer may issue a ``read`` with only the ``command`` and ``session`` parameters to read all points in their native dimenion formats.

----

Read - Quad-Tree Indexed Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------------------------------------------------------------------------+
| Command                                                                                |
+---------------------+------------+-----------------------------------------------------+
| Key                 | Type       | Value                                               |
+=====================+============+=====================================================+
| ``"command"``       | String     | ``"read"``                                          |
+---------------------+------------+-----------------------------------------------------+
| ``"session"``       | String     | Greyhound session ID                                |
+---------------------+------------+-----------------------------------------------------+
| (``"schema"``)      | String     | JSON stringified schema for return data             |
+---------------------+------------+-----------------------------------------------------+
| (``"bbox"``)        | Float[4]   | Bounding box to query                               |
+---------------------+------------+-----------------------------------------------------+
| (``"depthBegin"``)  | Integer    | Minimum quad tree depth from which to include points|
+---------------------+------------+-----------------------------------------------------+
| (``"depthEnd"``)    | Integer    | Quad-tree depth from which only points *less* than  |
|                     |            | this level will be included                         |
+---------------------+------------+-----------------------------------------------------+

Notes:
 - See `Read (Basics)`_ for information on the Greyhound response.
 - ``bbox``: Formatted as ``[xMin, yMin, xMax, yMax]``.  If omitted, returns points from the entire set.
 - ``depthBegin``: If omitted, defaults to zero.
 - ``depthEnd``: If omitted, then every tree level greater than or equal to ``depthBegin`` is included.
 - This query requires a quad-tree index to be created prior to reading, so the first quad-tree indexed ``read`` may take longer than usual to complete.  This may be completed in advance by Greyhound due to internal session sharing.
 - See `Taking Advantage of Indexing`_ for information on leveraging the quad-tree index.

----

Read - Quad-Tree Indexed Raster
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------------------------------------------------------------------------+
| Command                                                                                |
+---------------------+------------+-----------------------------------------------------+
| Key                 | Type       | Value                                               |
+=====================+============+=====================================================+
| ``"command"``       | String     | ``"read"``                                          |
+---------------------+------------+-----------------------------------------------------+
| ``"session"``       | String     | Greyhound session ID                                |
+---------------------+------------+-----------------------------------------------------+
| (``"schema"``)      | String     | JSON stringified schema for return data             |
+---------------------+------------+-----------------------------------------------------+
| ``"rasterize"``     | Integer    | Quad-tree level to rasterize                        |
+---------------------+------------+-----------------------------------------------------+

Notes:
 - See `Read (Raster Basics)`_ for information on the Greyhound response.
 - This query requires a quad-tree index to be created prior to reading, so the first quad-tree indexed ``read`` may take longer than usual to complete.  This may be completed in advance by Greyhound due to internal session sharing.

Important:
 - Results are in raster format.

----

Read - Generic Raster
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-------------------------------------------------------------------------------------------+
| Command                                                                                   |
+---------------------+---------------+-----------------------------------------------------+
| Key                 | Type          | Value                                               |
+=====================+===============+=====================================================+
| ``"command"``       | String        | ``"read"``                                          |
+---------------------+---------------+-----------------------------------------------------+
| ``"session"``       | String        | Greyhound session ID                                |
+---------------------+---------------+-----------------------------------------------------+
| (``"schema"``)      | String        | JSON stringified schema for return data             |
+---------------------+---------------+-----------------------------------------------------+
| ``"bbox"``          | Float[4]      | Bounding box to query                               |
+---------------------+---------------+-----------------------------------------------------+
| ``"resolution"``    | Integer[2]    | Resolution of the returned raster                   |
+---------------------+---------------+-----------------------------------------------------+

Notes:
 - See `Read (Raster Basics)`_ for information on the Greyhound response.
 - ``bbox``: Formatted as ``[xMin, yMin, xMax, yMax]``.
 - ``resolution``: Formatted as ``[xResolution, yResolution]``.
 - This query requires a quad-tree index to be created prior to reading, so the first quad-tree indexed ``read`` may take longer than usual to complete.  This may be completed in advance by Greyhound due to internal session sharing.

Important:
 - Results are in raster format.

----

Read - KD-Tree Indexed (Point-Radius)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------------------------------------------------------------------------+
| Command                                                                                |
+---------------------+------------+-----------------------------------------------------+
| Key                 | Type       | Value                                               |
+=====================+============+=====================================================+
| ``"command"``       | String     | ``"read"``                                          |
+---------------------+------------+-----------------------------------------------------+
| ``"session"``       | String     | Greyhound session ID                                |
+---------------------+------------+-----------------------------------------------------+
| (``"schema"``)      | String     | JSON stringified schema for return data             |
+---------------------+------------+-----------------------------------------------------+
| ``"x"``             | Float      | X coordinate of center point                        |
+---------------------+------------+-----------------------------------------------------+
| ``"y"``             | Float      | Y coordinate of center point                        |
+---------------------+------------+-----------------------------------------------------+
| (``"z"``)           | Float      | Z coordinate of center point                        |
+---------------------+------------+-----------------------------------------------------+
| ``"radius"``        | Float      | Query radius                                        |
+---------------------+------------+-----------------------------------------------------+

Notes:
 - See `Read (Basics)`_ for information on the Greyhound response.
 - ``z``: If omitted, the query is 2-dimensional, otherwise the query is 3-dimensional.
 - This query requires a KD-tree index to be created prior to reading, so the first KD-tree indexed ``read`` may take longer than usual to complete.  This may be completed in advance by Greyhound due to internal session sharing.  2-dimensional and 3-dimensional queries require different trees to be built, so a 2-dimensional ``read`` does not ensure that a 3-dimensional ``read`` will have its index pre-built.

----

Cancel
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TODO

----

Destroy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------------+
| Command                                                                     |
+---------------+------------+------------------------------------------------+
| Key           | Type       | Value                                          |
+===============+============+================================================+
| ``"command"`` | String     | ``"destroy"``                                  |
+---------------+------------+------------------------------------------------+
| ``"session"`` | String     | Greyhound session ID                           |
+---------------+------------+------------------------------------------------+

+-----------------------------------------------------------------------------------------+
| Response                                                                                |
+-------------------+------------+--------------------------------------------------------+
| Key               | Type       | Value                                                  |
+===================+============+========================================================+
| ``"command"``     | String     | ``"destroy"``                                          |
+-------------------+------------+--------------------------------------------------------+
| ``"status"``      | Integer    | ``1`` for success, else ``0``                          |
+-------------------+------------+--------------------------------------------------------+

TODO
    Descriptions

Working with Greyhound
===============================================================================

The Schema
-------------------------------------------------------------------------------

Session Schema
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The transfer schema used by Greyhound is a stringified JSON array of dimension information.  Each dimension entry contains:

+---------------+--------------------------------------------------------------------------------+
| Field         | Value                                                                          |
+===============+================================================================================+
| ``"name"``    | PDAL Dimension name.                                                           |
+---------------+--------------------------------------------------------------------------------+
| ``"type"``    | Dimension type.  Possible values: ``"signed"``, ``"unsigned"``, ``"floating"`` |
+---------------+--------------------------------------------------------------------------------+
| ``"size"``    | Dimension size in bytes.  Possible values: ``"1"``, ``"2"``, ``"4"``, ``"8"``  |
+---------------+--------------------------------------------------------------------------------+

An example return object from the ``schema`` call looks something like: ::

    "schema":
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
            "name": "GpsTime",
            "type": "floating",
            "size": "8"
        },
        {
            "name": "ScanAngleRank",
            "type": "floating",
            "size": "4"
        },
        {
            "name": "Intensity",
            "type": "unsigned",
            "size": "2"
        },
        {
            "name": "PointSourceId",
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
            "name": "ScanDirectionFlag",
            "type": "unsigned",
            "size": "1"
        },
        {
            "name": "EdgeOfFlightLine",
            "type": "unsigned",
            "size": "1"
        },
        {
            "name": "Classification",
            "type": "unsigned",
            "size": "1"
        },
        {
            "name": "UserData",
            "type": "unsigned",
            "size": "1"
        }
    ]

This schema represents the native PDAL dimensions and storage types inherent to the requested session.  However, not all of these dimensions may be necessary for a given ``read``, and retrieving needed dimensions in their native types may not be ideal for every situation.



Manipulating the Schema
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For various reasons, a client may wish to ``read`` with a different schema than the native schema.  For example,

 - Reducing transfer bandwidth by lowering the resolution of some dimensions (e.g. ``double`` to ``float`` type in C++)
 - Needing only a subset of the dimensions from the entire available schema
 - Wanting dimensions expressed as different types than the native types

Therefore Greyhound provides the ability to request the results of a ``read`` in a flexible way.  By supplying a ``schema`` parameter in the ``read`` request, the resulting ``read`` will format its binary data in accordance with the requested ``schema`` instead of the default.  The default schema can be queried with the `Schema` request.

Dimension names should be a subset of those returned from ``schema``.  Names that do not exist in the current session will be silently ignored by Greyhound as if they were not present in the requested ``schema``.

Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A visual rendering client may only with to retrieve dimensions relevant to displaying the data.  This example ``schema``, to be included in each ``read`` request, demonstrates the client's ability to

 - retrieve only a subset of all existing dimensions in the session
 - halve the bandwidth required to transmit the ``X``, ``Y``, and ``Z`` dimensions by requesting them as 4 bytes rather than the native 8

::

    "schema":
    [
        {
            "name": "X",
            "type": "floating",
            "size": "4"
        },
        {
            "name": "Y",
            "type": "floating",
            "size": "4"
        },
        {
            "name": "Z",
            "type": "floating",
            "size": "4"
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
        }
    ]

Raster Metadata
-------------------------------------------------------------------------------

Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this scenario we will get a raster of only the ``Z`` dimension values.  So the ``schema`` parameter transmitted with the ``read`` request may look like:

::

    "schema":
    [
        {
            "name": "X",
        },
        {
            "name": "Y",
        },
        {
            "name": "Z",
            "type": "floating",
            "size": "4"
        }
    ]


The resulting ``rasterMeta`` provided in the ``read`` result from Greyhound may look something like:

::

    "rasterMeta":
    {
        "xBegin": 500,
        "xStep":  25,
        "xNum":   4,
        "yBegin": 3000,
        "yStep":  50,
        "yNum":   3
    }

Given these two parameters, we can determine that:
 - The record size for each point is 4 bytes (``Z`` only).
 - The bounding box for these results is: ``(xMin, yMin, xMax, yMax) = (500, 3000, 575, 3100)``.
 - The binary data is 48 bytes long (this information also arrives in ``numBytes``).
 - The binary buffer structure looks like:

+-----------------+-----------------+-----------------+-----------------+
| ``Byte offset``: (``X``, ``Y``)                                       |
+=================+=================+=================+=================+
| 00: (500, 3000) | 04: (525, 3000) | 08: (550, 3000) | 12: (575, 3000) |
+-----------------+-----------------+-----------------+-----------------+
| 16: (500, 3050) | 20: (525, 3050) | 24: (550, 3050) | 28: (575, 3050) |
+-----------------+-----------------+-----------------+-----------------+
| 32: (500, 3100) | 36: (525, 3100) | 40: (550, 3100) | 44: (575, 3100) |
+-----------------+-----------------+-----------------+-----------------+

Pseudocode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The raster can be read programmatically similar to the pseudocode below, assuming that the raster contains only 4-byte floating ``Z`` values.

::

    // Schema size minus X and Y sizes.  In this case equal to 4.
    int recordSize = <reduced schema size>;

    // Binary data received from Greyhound.
    const unsigned char* buffer;

    // Raster meta object received from Greyhound.
    RasterMeta rasterMeta;

    // Container for points.
    vector<Point> points;

    for (int yIndex = 0; yIndex < yNum; ++y)
    {
        for (int xIndex = 0; xIndex < xNum; ++x)
        {
            int zOffset = recordSize * (yIndex * rasterMeta.xNum + xIndex);

            float x = meta.xBegin + (xIndex * meta.xStep);
            float y = meta.yBegin + (yIndex * meta.yStep);
            float z = buffer.getDoubleFromIndex(zOffset);

            points.push_back(Point(x, y, z));
        }
    }

Taking Advantage of Indexing
-------------------------------------------------------------------------------

TODO
 - Can progressively query deeper levels of the quad tree to fill in detail.
 - Explain tree depth centering.

Deploying Greyhound
===============================================================================

Setting up the Server
-------------------------------------------------------------------------------

TODO

Configuring Greyhound Settings
-------------------------------------------------------------------------------

TODO

