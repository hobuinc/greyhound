#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run
#

# setup packages which are a nightmare to set through vagrant provisioning
sudo gem install foreman
sudo npm install -g hipache

# issue npm install in each of the node.js projects
for P in request-handler websocket-handler dist-handler ; do
	cd $P ; npm install ; cd ..
done

# build our pdal-session binary
make
