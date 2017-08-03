FROM connormanning/entwine:1.1
MAINTAINER Connor Manning <connor@hobu.co>

ENV CC gcc
ENV CXX g++

RUN apt-get update && apt-get install -y wget && \
    (wget -qO- https://deb.nodesource.com/setup_4.x | bash -) && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/* && \
    npm install -g node-gyp mocha

ADD https://api.github.com/repos/hobu/greyhound/commits /tmp/bust-cache
RUN git clone https://github.com/hobu/greyhound.git /var/greyhound && \
    cd /var/greyhound && \
    git checkout d6ef848a757d1d74fb20b5c9ca6ab1cc33e01cc0 && \
    npm install -g .

EXPOSE 8080
EXPOSE 443
VOLUME /opt/data
VOLUME /greyhound

ENTRYPOINT ["greyhound"]

# Sample invocation:
#       docker run -it -p 8080:80 -v ~/greyhound:/opt/data connormanning/greyhound

