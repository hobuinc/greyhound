#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run
#

# setup packages which are a nightmare to set through vagrant provisioning
sudo gem install foreman --no-rdoc --no-ri
sudo npm install -g hipache

if [ ! -d "/usr/local/include/websocketpp" ] ; then
	# build and install websocketpp
	git clone https://github.com/zaphoyd/websocketpp.git /tmp/websocketpp &&
		cd /tmp/websocketpp &&
		cmake -G "Unix Makefiles" && make &&
		sudo make install && rm -rf /tmp/websocketpp && cd -
else
	echo websocketpp already isntalled
fi

# issue npm install in each of the node.js projects
for P in request-handler websocket-handler dist-handler examples/js ; do
	cd $P ; npm install ; cd -
done

# build our pdal-session binary
echo Building C++ components...
make all
