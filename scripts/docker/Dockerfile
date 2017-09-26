FROM connormanning/entwine
MAINTAINER Connor Manning <connor@hobu.co>

ENV CC gcc
ENV CXX g++

# Node.js is used only for testing - it is not a Greyhound dependency.
RUN apt-get update && apt-get install -y wget && \
    (wget -qO- https://deb.nodesource.com/setup_6.x | bash -) && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/* && \
    npm install -g mocha

RUN apt-get update && apt-get install -y libboost-all-dev
RUN git clone https://github.com/eidheim/Simple-Web-Server.git \
    /var/simple-web && \
    cd /var/simple-web && mkdir build && cd build && \
    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release .. && \
    make -j4 && \
    make install

ADD https://api.github.com/repos/hobu/greyhound/commits /tmp/bust-cache
RUN git clone https://github.com/hobu/greyhound.git /var/greyhound && \
    cd /var/greyhound && mkdir build && cd build && \
    cmake -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release .. && \
    make install

EXPOSE 8080
EXPOSE 8443
VOLUME /opt/data
VOLUME /greyhound

ENTRYPOINT ["greyhound"]

# Sample invocation:
#       docker run -it -p 8080:8080 -v ~/greyhound:/opt/data \
#           connormanning/greyhound

