process.title = 'greyhound';

var console = require('clim')(),
    fs = require('fs'),
    path = require('path'),
    join = path.join,
    minify = require('jsonminify'),

    Controller = require(join(__dirname, '/controller')).Controller,
    WsHandler = require(join(__dirname, '/interfaces/ws/handler')).WsHandler,
    HttpHandler = require(
            join(__dirname, '/interfaces/http/handler')).HttpHandler,
    configExists = (() => {
        try { fs.accessSync(join(__dirname, '../config.json')); return true; }
        catch (e) { return false; }
    })(),
    configPath = configExists ? '../config.json' : '../config.defaults.json',
    config = (() => {
        return JSON.parse(minify(fs.readFileSync(
                    join(__dirname, configPath), { encoding: 'utf8' })));
    })()
    ;

if (config.auth) {
    var maybeAddSlashTo = (s) => s.slice(-1) == '/' ? s : s + '/';
    config.auth.path = maybeAddSlashTo(config.auth.path);

    if (typeof config.auth.cacheMinutes == 'number') {
        config.auth.cacheMinutes = {
            good: config.auth.cacheMinutes,
            bad: config.auth.cacheMinutes
        };
    }

    if (!config.auth.cacheMinutes.good) config.auth.cacheMinutes.good = 1;
    if (!config.auth.cacheMinutes.bad)  config.auth.cacheMinutes.bad  = 1;
}

if (!configExists) console.log('Using default config');

var controller = new Controller(config);

process.nextTick(function() {
    if (config.ws && config.ws.port) {
        var wsHandler = new WsHandler(controller, config.ws.port);
        wsHandler.start();
    }

    if (config.http && (config.http.port || config.http.securePort)) {
        var http = config.http;
        var creds = null;

        if (http.keyFile && http.certFile && http.securePort) {
            var getPath = (file) => {
                if (file == 'key.pem' || file == 'cert.pem') {
                    return path.join(__dirname, '../', file);
                }
                else return file;
            };

            var key = fs.readFileSync(getPath(http.keyFile));
            var cert = fs.readFileSync(getPath(http.certFile));

            creds = { key: key, cert: cert };
        }

        var httpHandler = new HttpHandler(
            controller,
            http.port,
            http.securePort,
            creds);

        httpHandler.start();
    }
});

