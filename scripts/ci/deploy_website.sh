#!/bin/bash

builddir=$1


echo "deploying docs for $TRAVIS_BUILD_DIR/docs"

docker run -e "AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY" -e "AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID" -v $TRAVIS_BUILD_DIR:/data -w /data/doc hobu/entwine-docs aws s3 sync ./build/html/ s3://greyhound.io --acl public-read

