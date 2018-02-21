FROM connormanning/entwine
MAINTAINER Connor Manning <connor@hobu.co>

RUN \
    echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> \
        /etc/apk/repositories; \
    apk update; \
    apk add --no-cache --virtual .build-deps \
        alpine-sdk \
        libexecinfo-dev \
        libunwind-dev \
        boost-dev \
        laz-perf-dev \
        curl-dev \
        git openssh; \
    apk add --no-cache \
        nodejs-npm \
        libexecinfo \
        libunwind \
        boost \
        curl \
        nodejs; \
    npm install -g mocha; \
    git clone https://github.com/eidheim/Simple-Web-Server.git /var/simple-web;\
    /var/simple-web ;\
    cd /var/simple-web && mkdir build && cd build ;\
    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release .. ;\
    make -j4 ;\
    make install; \
    git clone https://github.com/hobu/greyhound.git /var/greyhound ;\
    cd /var/greyhound && mkdir build && cd build ;\
    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo .. ;\
    make install; \
    apk del .build-deps;

EXPOSE 8080
EXPOSE 8443

ENTRYPOINT ["greyhound"]

# Sample invocation:
#       docker run -it -p 8080:8080 -v ~/greyhound:/opt/data \
#           connormanning/greyhound

