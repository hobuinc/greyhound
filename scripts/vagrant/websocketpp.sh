#!/bin/bash -e
# build and install websocketpp
#

git clone https://github.com/zaphoyd/websocketpp.git /tmp/websocketpp
cd /tmp/websocketpp

cmake -G "Unix Makefiles"
make
sudo make install
cd -
rm -rf /tmp/websocketpp
