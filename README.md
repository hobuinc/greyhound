# GREYHOUND

An HTTP point cloud streaming framework for dynamic web services and native applications.

![pointcloud](doc/logo.png)

# Getting started
```
npm install -g greyhound-server
```

# About the project
_Greyhound_ provides dynamic querying and streaming over HTTP of [Entwine](https://github.com/connormanning/entwine) indexed data.  See it in action at [speck.ly](http://speck.ly) and [potree.entwine.io](http://potree.entwine.io).
_Greyhound_ is powered by [PDAL](http://www.pointcloud.org/) which handles data abstraction for a wide variety of formats.  See the [client documentation](https://github.com/hobu/greyhound/blob/master/doc/clientDevelopment.rst) and the [administrator documentation](https://github.com/hobu/greyhound/blob/master/doc/administration.rst) for more details on development and deployment.

A simple [RESTful](https://en.wikipedia.org/wiki/Representational_state_transfer) HTTP protocol, or its equivalent [WebSocket](https://www.websocket.org/) interface, is used to query and stream points in a format specified by the client.

# How to hack?

For now, see the usage instructions in the [entwine repository](https://github.com/connormanning/entwine).

# License
_Greyhound_ is under **MIT** license and is Copyright [Howard Butler](http://hobu.co), [Uday Verma](https://github.com/verma), and [Connor Manning](https://github.com/connormanning).

