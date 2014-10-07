proxy: sudo /usr/sbin/haproxy -f /var/greyhound/pre/frontend-proxy/haproxy.cfg -d
mongo: mongod --dbpath /home/vagrant/data --port 21212 --logpath /home/vagrant/log.txt
ws-proxy: /usr/bin/hipache -c /var/greyhound/pre/frontend-proxy/hipache-config.json
web: env PORT=8080 node /var/greyhound/pre/web/app.js

