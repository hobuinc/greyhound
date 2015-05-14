#!/bin/sh
#
# A script to set up the Greyhound environment for standalone use.
#

# Install NPM packages, build our pdal-session binary, and build C++ examples.
cd /vagrant/
make all
make install

# Set up auto-relaunch of Greyhound components, and launch them.
echo Setting up autolaunch
greyhound auto
echo Starting Greyhound!
greyhound start

# Give Greyhound a few seconds to start up.
sleep 5
greyhound status

cd -

