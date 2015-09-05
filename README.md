# GREYHOUND

A pointcloud querying and streaming framework over HTTP or websockets for the web and your native apps.

![pointcloud](pointcloud.png)

# Tell me more
_Greyhound_ is powered by [PDAL](http://www.pointcloud.org/) which handles data abstraction for a wide variety of formats.  See the [client documentation](https://github.com/hobu/greyhound/blob/master/doc/clientDevelopment.rst) and the [administrator documentation](https://github.com/hobu/greyhound/blob/master/doc/administration.rst) for more details on development and deployment.

A simple [RESTful](https://en.wikipedia.org/wiki/Representational_state_transfer) HTTP protocol, or its equivalent [WebSocket](https://www.websocket.org/) interface, is used to query and stream points in a format specified by the client.

# How to hack?
You will need [Vagrant](http://www.vagrantup.com/) installed to play with _Greyhound_.  Once you check out the source code, browse to the checked out directory and do a:

	vagrant up

This will start the virtual machine and launch _Greyhound_.  The first time might take a while as it downloads virtual OS images and dependencies.

_Greyhound_ allows clients to stream from datasets stored locally, over HTTP, or even indexed into S3 storage.  After `vagrant up` completes, you should be able to do some dynamic browsing of a small pre-built index that ships with _Greyhound_:

 - TODO URL connecting to small set on localhost:8080

## Going further
To get more functionality than looking at the sample point clouds in your browser, you'll need to SSH into your Vagrant machine with:

	vagrant ssh

Once you are connected to the virtual machine, you have control over the _Greyhound_ stack with:

- `greyhound start` Start _Greyhound_.
- `greyhound stop`  Stop _Greyhound_.
- `greyhound log`   View real-time server log while you browse a set.

# License
_Greyhound_ is under **MIT** license and is Copyright [Howard Butler](http://hobu.co), [Uday Verma](https://github.com/verma), and [Connor Manning](https://github.com/connormanning).

