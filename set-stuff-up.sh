#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run
#

setup_base_packages() {
    echo Installing foreman
	sudo gem install foreman --no-rdoc --no-ri
	sudo npm install -g hipache
}

setup_npm_packages() {
	# issue npm install in each of the node.js projects
	for P in /vagrant/web /vagrant/request-handler /vagrant/websocket-handler \
      /vagrant/dist-handler /vagrant/db-handler /vagrant/examples/js ; do
		cd $P ; npm install ; cd -
	done
}

setup_cpp_components() {
	# build our pdal-session binary
    cd /vagrant
	make all
}

#setup_greyhound() {
    # initialize the database with sample file and launch services

    # TODO Launch Greyhound in the background.
    # TODO Need some method to poll until all Greyhound components are
    # launched before we can perform the PUT. 

    # Get the database started with a pre-written sample.
    # TODO Uncomment when above steps are done.
    #/vagrant/examples/cpp/put-pipeline
#}

setup_base_packages
setup_npm_packages
setup_cpp_components
#setup_greyhound

