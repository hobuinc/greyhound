from	ubuntu:12.04
run	echo "deb http://archive.ubuntu.com/ubuntu precise main universe" > /etc/apt/sources.list
run	apt-get -y update
run	apt-get -y install wget git redis-server supervisor python python-software-properties

RUN add-apt-repository -y ppa:chris-lea/node.js
RUN apt-get update
RUN apt-get install -y nodejs

run	npm install hipache -g

run mkdir -p /app/hipache
run	mkdir -p /var/log/supervisor

add ./hipache-config.json /app/hipache/config.json
add	./supervisord.conf /etc/supervisor/conf.d/supervisord.conf

expose	:80
expose	:6379

cmd	supervisord -n
