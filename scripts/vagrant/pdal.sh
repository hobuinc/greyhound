NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

git clone https://github.com/PDAL/PDAL.git pdal
cd pdal
git checkout 372a4b3598e658033c247a72fb3607d451ddbd48
cmake   -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_ICONV=ON \
        -DWITH_GEOTIFF=ON \
        -DWITH_LASZIP=ON \
        -DWITH_LAZPERF=ON \
        -DLAZPERF_INCLUDE_DIR=/usr/local/include/tlaz/ \
        -DWITH_LIBXML2=ON \
        -DWITH_PYTHON=ON \
        -DWITH_TESTS=OFF

make -j $NUMTHREADS
sudo make install

