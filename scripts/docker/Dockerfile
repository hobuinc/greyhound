FROM connormanning/entwine:latest
MAINTAINER Connor Manning <connor@hobu.co>

ENV CC gcc
ENV CXX g++

RUN apt-get update && apt-get install -y wget && \
    (wget -qO- https://deb.nodesource.com/setup_4.x | bash -) && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

ADD http://www.timeapi.org/utc/now /tmp/bust-cache
RUN git clone https://github.com/hobu/greyhound.git && \
    cd greyhound && \
    npm install -g .

EXPOSE 8080
EXPOSE 443
VOLUME /opt/data

CMD greyhound

# Sample invocation:
#       docker run -it -p 8080:80 -v ~/greyhound:/opt/data connormanning/greyhound

