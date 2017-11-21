docker run -t -v $TRAVIS_BUILD_DIR:/opt/greyhound \
    --entrypoint bash \
    connormanning/greyhound -c " \
        cd /opt/greyhound && mkdir -p build && cd build && \
        cmake -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
        make -j4 &&
        ../scripts/generate-test-data.sh && \
        (nohup ./greyhound/greyhound -d ../data > greyhound-log.txt &) && \
        cd ../test && npm install && npm test"

