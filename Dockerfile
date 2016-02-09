FROM hobu/entwine:latest
MAINTAINER Connor Manning <connor@hobu.co>
ARG branch=entwine

ENV CC clang
ENV CXX clang++
ARG branch=entwine

RUN apt-get update && apt-get install -y supervisor
RUN rm -rf /var/lib/apt/lists/*

ENV nodeVer="4.2.1"
ENV nodeUrl="http://nodejs.org/dist/v${nodeVer}/node-v${nodeVer}-linux-x64.tar.gz"
RUN mkdir -p /tmp/nodejs && \
    wget -qO - ${nodeUrl} | tar zxf - --strip-components 1 -C /tmp/nodejs && \
    cd /tmp/nodejs && \
    cp -r * /usr

RUN npm install -g node-gyp && npm update npm -g && npm cache clean

RUN git clone "https://github.com/hobu/greyhound.git" && \
    cd greyhound && \
    git checkout ${branch} && \
    make && \
    make install

EXPOSE 80
EXPOSE 443
EXPOSE 8989

# Sample invocation:
#       docker run -it -p 80:80 -p 443:443 -p 8989:8989 \
#           -v ~/greyhound/:/opt/greyhound \
#           greyhound \
#               /bin/bash -c \
#               "cp /opt/greyhound/config.js /var/greyhound/ && \
#               greyhound dockerstart && greyhound log"

