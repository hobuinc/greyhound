#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run.
#

setup_base_packages() {
    echo Installing foreman
	sudo gem install foreman --no-rdoc --no-ri
	sudo npm install -g hipache
    sudo npm install -g nodeunit

    # Install png++
    wget http://download.savannah.nongnu.org/releases/pngpp/png++-0.2.5.tar.gz
    tar zxvf png++-0.2.5.tar.gz
    cd png++-0.2.5
    sudo make install PREFIX=/usr
    cd -;
}

setup_npm_packages() {
	# Issue npm install in each of the node.js projects.
	for P in \
        /vagrant/web \
        /vagrant/session-handler \
        /vagrant/websocket-handler \
        /vagrant/dist-handler \
        /vagrant/db-handler \
        /vagrant/examples/js \
        /vagrant/test \
        /vagrant/common \
        ; do
		cd $P ; npm install ; cd -
	done
}

setup_cpp_components() {
	# Build our pdal-session binary.
    cd /vagrant
	make all
}

setup_greyhound() {
    # Initialize the database with sample files and launch services.
    echo Making DB dir
    su -l vagrant -c "mkdir -p /home/vagrant/data"

    # Launch Greyhound.
    chmod 755 /vagrant/gh
    echo Launching Greyhound
    su -l vagrant -c "/vagrant/gh start"

    # TODO Need some method to ensure that all Greyhound components are
    # launched before we can perform the PUT.  For now hack in a sleep.
    sleep 10

    # Get the database started with a pre-written sample.
    su -l vagrant -c "/vagrant/examples/cpp/put-pipeline"

    # Also add the large sample, needed for testing cancel functionality.
    su -l vagrant -c "/vagrant/examples/cpp/put-pipeline /vagrant/examples/data/half-dome.xml"
}

setup_base_packages
setup_npm_packages
setup_cpp_components
setup_greyhound

