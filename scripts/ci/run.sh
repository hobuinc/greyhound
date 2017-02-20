docker run -t \
    -v $TRAVIS_BUILD_DIR:/opt/greyhound \
    --entrypoint /bin/bash \
    connormanning/greyhound -c " \
        NODE_ENV=debug cd /opt/greyhound && npm run generate-test-data && \
        npm install && node-gyp configure && \
        node-gyp build --debug && \
        (NODE_ENV=debug nohup ./src/app.js --debug > greyhound-log.txt &) && \
        echo Starting Greyhound tests in a few seconds... && sleep 10 && \
        npm test"

