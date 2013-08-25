#!/bin/sh
#
# A script to setup environment and make sure everything is ready to run
#

# setup packages which are a nightmare to set through vagrant provisioning
setup_base_packages() {
	sudo gem install foreman --no-rdoc --no-ri
	sudo npm install -g hipache
}

setup_websocketpp() {
	# setup websocketpp
	if [ ! -d "/usr/local/include/websocketpp" ] ; then
		# build and install websocketpp
		git clone https://github.com/zaphoyd/websocketpp.git /tmp/websocketpp &&
			cd /tmp/websocketpp &&
			cmake -G "Unix Makefiles" && make &&
			sudo make install && rm -rf /tmp/websocketpp && cd -
	else
		echo websocketpp already installed
	fi
}


# setup PDAL
setup_pdal() {
	if [ ! -f /usr/include/geotiff.h ] ; then
		# install libgeotiff from sources
		mkdir -p /tmp/geotiff && wget -qO - http://download.osgeo.org/geotiff/libgeotiff/libgeotiff-1.4.0.tar.gz | tar \
			zxf - --strip-components 1 -C /tmp/geotiff && \
			cd /tmp/geotiff && \
			./configure --prefix=/usr && make && sudo make install && cd -
	else
		echo libgeotiff seems to be already available
	fi


	if [ ! -d /usr/local/include/pdal ] ; then
		# remove any stale libs
		if [ -d /tmp/PDAL ] ; then
			rm -rf /tmp/PDAL
		fi

		git clone https://github.com/PDAL/PDAL.git /tmp/PDAL

		PWD=`pwd`
		cd /tmp/PDAL && mkdir _build && cd _build &&
			cmake \
			-DWITH_FLANN=ON \
			-DWITH_GDAL=ON \
			-DWITH_GEOTIFF=ON \
			-DWITH_LIBXML2=ON \
			-DWITH_PGPOINTCLOUD=ON \
			.. && make && sudo make install
		cd "$PWD"
	else
		echo pdal seems to be already available
	fi

	sudo ldconfig
}

setup_npm_packages() {
	# issue npm install in each of the node.js projects
	for P in request-handler websocket-handler dist-handler examples/js ; do
		cd $P ; npm install ; cd -
	done
}

setup_cpp_components() {
	# build our pdal-session binary
	make all
}


setup_base_packages
setup_websocketpp
setup_pdal
setup_npm_packages
setup_cpp_components
