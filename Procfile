proxy: sudo /usr/sbin/haproxy -f ./frontend-proxy/haproxy.cfg -d
mongo: mongod --dbpath /home/vagrant/data --port 21212 --logpath /home/vagrant/log.txt
ws-proxy: /usr/bin/hipache -c ./frontend-proxy/hipache-config.json > /vagrant/frontend-proxy/hipache-log.txt

dist: node ./dist-handler/app.js
sh: node ./session-handler/app.js
ws: node ./websocket-handler/app.js
web: env PORT=8080 node ./web/app.js

