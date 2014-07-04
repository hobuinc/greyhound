#!/bin/bash -e
# Installs hexer library

# TODO: Temporarily use this fork due to https://github.com/hobu/hexer/issues/13
git clone https://github.com/connormanning/hexer.git
cd hexer
cmake . -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
