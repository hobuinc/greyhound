#!/bin/sh
#
# A script to set up the Greyhound environment for standalone use.
#

# Install NPM packages, build our pdal-session binary, and build C++ examples.
cd /vagrant/
make all
make install STANDALONE=TRUE

# Set up auto-relaunch of Greyhound components, and launch them.
echo Setting up autolaunch
greyhound auto
echo Starting Greyhound!
greyhound start

# Give Greyhound a few seconds to start up.  We can check the status via Redis
# manually, but for a programmatic method we will just wait a bit.
sleep 5

# Pre-load pipelines.
/vagrant/examples/cpp/put-pipeline
/vagrant/examples/cpp/put-pipeline examples/data/half-dome.xml

cd -

