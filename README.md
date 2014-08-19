# GREYHOUND

A pointcloud querying and streaming framework over websockets for the web and your native apps.

![pointcloud](pointcloud.png)

# Tell me more
_Greyhound_ is powered by [PDAL](http://www.pointcloud.org/) in the background which it uses to query points out of a point cloud source. The queries flow through a variety of systems put in place for scalability and session management.  PDAL sessions are maintained in stand-alone binaries which exist for as long as sessions are active.  See the [overview](https://github.com/hobu/greyhound/blob/master/doc/overview.rst) document for more details.

A json-over-websockets protocol is used to initiate a session and query points.  Point data is streamed as binary data from the server.  See the _examples_ directory on how to query and read data, and how to store pipelines within _Greyhound_.

# How to hack?
You will need [Vagrant](http://www.vagrantup.com/) installed to play with _Greyhound_.  Once you check out the source code, browse to the checked out directory and do a:

	vagrant up

This will start the virtual machine and launch _Greyhound_.  The first time you do it, it may take a while to set up the virtual machine with all the needed components (some installed from ubuntu repos, some built manually).

_Greyhound_ allows clients to make use of pipelines that have been previously stored within the _Greyhound_ database as their pointcloud source.  These pipelines must be placed into the database before they may be used, and then selected with the ID that _Greyhound_ assigns.  The database is pre-initialized with a sample pipeline during the `vagrant up` procedure.  To have pointcloud data immediately rendered in a browser once the process finishes, you can navigate to:

    http://localhost:8080/data/d4f4cc08e63242a201de6132e5f54b08

_Greyhound_ also supports some spatially indexed queries via URL query parameters (these links operate on a large pipeline):

- [Point-radius query](http://localhost:8080/data/4a14c92da9bc6df24400d69fa9add65a?radius=400&geo=%7B%22type%22:%22Point%22,%22coordinates%22:%5B276488.2105233709,4179808.998997613,2029.596267072244%5D%7D)
- [BBox and quadtree search depth query](http://localhost:8080/data/4a14c92da9bc6df24400d69fa9add65a?geo={%22bbox%22:[276400,4179000,277100,4179700]}&depthEnd=10)

## Going further
To get more functionality than looking at the sample point cloud in your browser, you'll need to SSH into your Vagrant machine with:

	vagrant ssh

Once you are connected to the virtual machine, you have control over the _Greyhound_ stack with:

- `./gh start [-q]`  Start all _Greyhound_ components.  The optional `-q` flag will suppress console output from within _Greyhound_, useful for getting a clean output from the unit tests.
- `./gh stop`   Stop all _Greyhound_ components.

When _Greyhound_ services are running, you can try out the sample C++ client code (all commands operate on the sample pipeline if no argument is provided) or run the unit tests to verify the integrity of your Greyhound stack:

- `./examples/cpp/put-pipeline [/path/to/pipeline.xml]` Write the chosen pipeline into the _Greyhound_ database.  The assigned _Greyhound_ ID will be printed to stdout.
- `./examples/cpp/get-points [GreyhoundPipelineId]`     Get points specified by the chosen pipeline ID.
- `nodeunit test/unit.js`                               Run all _Greyhound_ unit tests.

# Other info
The client side web application uses pretty rudimentary logic to detect whether color information is available.  Your output needs to have the XYZ dimensions as signed 4-byte integers and, optionally, the RGB dimensions for color information as unsigned 2-byte shorts.  A record size of 18 indicates that color information is available (3 * 4 + 2 * 3) while a record size of 12 indicates no color information.

This will change as we finalize schema transfer.

# License
_Greyhound_ is under **MIT** license and is Copyright [Howard Butler](http://hobu.biz), [Uday Verma](https://github.com/verma), and [Connor Manning](https://github.com/connormanning).

