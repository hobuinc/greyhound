#!/bin/bash -e
# Installs hexer library

git clone https://github.com/hobu/hexer.git
cd hexer
# TODO
git checkout 8bd826d395120c6da809b95f04927b97ad77da53
cmake . -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
