var m = require('./index');

m.disco.add("sup", function(err, service) {
    console.log("sup services added for port", service.port);
    setTimeout(function() {
        service.remove();
    }, 20000);
});

var watcher = m.disco.watchForService("sup");

watcher.on('register', function(s) {
    console.log('registered', s);
});

watcher.on('unregister', function(s) {
    console.log('unregistered', s);
});

setInterval(function() {
    m.disco.get("sup", function(err, svc) {
        console.log(svc);
    });
}, 2000);
