#!/usr/bin/env bash

# Install node.js
nodeVersion="0.10.33"
nodeUrl="http://nodejs.org/dist/v$nodeVersion/node-v$nodeVersion-linux-x64.tar.gz"
echo Provisioning node.js version $nodeVersion...
mkdir -p /tmp/nodejs
wget -qO - $nodeUrl | tar zxf - --strip-components 1 -C /tmp/nodejs
cd /tmp/nodejs
cp -r * /usr
cd -

apt-get update
apt-get install -q -y python-software-properties

# Add PPAs
add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
add-apt-repository -y ppa:boost-latest/ppa

apt-get update

# Install apt-gettable packages
pkg=(
    git
    ruby
    build-essential
    libjsoncpp-dev
    pkg-config
    redis-server
    cmake
    libflann-dev
    libgdal-dev
    libpq-dev
    libproj-dev
    libtiff4-dev
    haproxy
    libgeos-dev
    python-all-dev
    python-numpy
    libxml2-dev
    libboost-all-dev
    libbz2-dev
    libsqlite0-dev
    cmake-curses-gui
    screen
    postgis
    libcunit1-dev
    postgresql-server-dev-9.3
    postgresql-9.3-postgis-2.1
    libgeos++-dev
)

apt-get install -q -y -V ${pkg[@]}

# Install gems
gem install foreman --no-rdoc --no-ri; npm install -g hipache nodeunit node-gyp

npm update npm -g
npm cache clean

