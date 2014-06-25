#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run
#

# setup packages which are a nightmare to set through vagrant provisioning
setup_base_packages() {
	sudo gem install foreman --no-rdoc --no-ri
	sudo npm install -g hipache
}

setup_npm_packages() {
	# issue npm install in each of the node.js projects
	for P in web request-handler websocket-handler dist-handler db-handler examples/js ; do
		cd $P ; npm install ; cd -
	done
}

setup_cpp_components() {
	# build our pdal-session binary
	make all
}


setup_base_packages
setup_npm_packages
setup_cpp_components
