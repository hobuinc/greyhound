#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run.
#

setup_base_packages() {
    echo Installing foreman
	sudo gem install foreman --no-rdoc --no-ri
	sudo npm install -g hipache
}

setup_npm_packages() {
	# Issue npm install in each of the node.js projects.
	for P in /vagrant/web /vagrant/request-handler /vagrant/websocket-handler \
      /vagrant/dist-handler /vagrant/db-handler /vagrant/examples/js ; do
		cd $P ; npm install ; cd -
	done
}

setup_cpp_components() {
	# Build our pdal-session binary.
    cd /vagrant
	make all
}

setup_greyhound() {
    # Initialize the database with sample file and launch services.

    # Launch Greyhound.
    /vagrant/gh start

    # TODO Need some method to ensure that all Greyhound components are
    # launched before we can perform the PUT.  For now hack in a sleep.
    sleep 10

    # Get the database started with a pre-written sample.
    /vagrant/examples/cpp/put-pipeline
}

setup_base_packages
setup_npm_packages
setup_cpp_components
setup_greyhound

