proxy: sudo /usr/sbin/haproxy -f ./frontend-proxy/haproxy.cfg -d
ws-proxy: /usr/bin/hipache -c ./frontend-proxy/hipache-config.json
dist: node ./dist-handler/app.js
rh: node ./request-handler/app.js
ws: node ./websocket-handler/app.js
web: env PORT=8080 node ./web/app.js
db: node ./db-handler/app.js

