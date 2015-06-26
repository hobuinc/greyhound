#!/usr/bin/env bash

NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

git clone https://github.com/PDAL/PDAL.git pdal
cd pdal
git checkout 5207a58820626ca2051d7ec802829f46105939de
cmake   -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_LASZIP=ON \
        -DWITH_LAZPERF=ON \
        -DWITH_TESTS=OFF

make -j $NUMTHREADS
sudo make install

