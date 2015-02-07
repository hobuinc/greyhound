NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

git clone https://github.com/PDAL/PDAL.git pdal
cd pdal
git checkout c70287817ebe90573578657d4b9912f970cec468
cmake   -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_GEOTIFF=ON \
        -DWITH_LASZIP=ON \
        -DWITH_LAZPERF=ON \
        -DWITH_TESTS=OFF

make -j $NUMTHREADS
sudo make install

