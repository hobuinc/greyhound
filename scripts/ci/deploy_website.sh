#!/bin/bash

builddir=$1
destdir=$2
DATE=$(date +'%y.%m.%d %H:%M:%S')

git clone git@github.com:hobu/greyhound.git $destdir/pdaldocs
cd $destdir/pdaldocs
git checkout gh-pages


cd $builddir/html
cp -rf * $destdir/pdaldocs

cd $builddir/latex/
cp PDAL.pdf $destdir/pdaldocs

cd $destdir/pdaldocs
git config user.email "pdal@hobu.net"
git config user.name "PDAL Travis docsbot"

git add -A
git commit -m "update with results of commit https://github.com/hobu/greyhound/commit/$TRAVIS_COMMIT for ${DATE}"
git push origin master

