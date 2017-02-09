#!/usr/bin/env bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $DIR/../data

# Normally we'd glob here, but that's not possible with the HTTP interface.
TESTING_SHALLOW=true entwine build \
    -i \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/ned.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/neu.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/nwd.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/nwu.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/sed.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/seu.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/swd.laz" \
        "https://s3.amazonaws.com/hobu-lidar/greyhound-test-data/ellipsoid/swu.laz" \
    -o \
        ellipsoid

