#!/usr/bin/env node

process.title = 'greyhound';

var console = require('clim')(),
    fs = require('fs'),
    path = require('path'),
    join = path.join,
    minify = require('jsonminify'),
    argv = require('minimist')(process.argv.slice(2)),

    Controller = require('./controller').Controller,
    HttpHandler = require('./interfaces/http').HttpHandler,
    usingDefaultConfig = false,
    configPath = (() => {
        if (argv.c) {
            console.log('Using config at', argv.c);
            return argv.c;
        }
        else {
            usingDefaultConfig = true;
            console.log('Using default config');
            return join(__dirname, 'config.defaults.json');
        }
    })(),
    config = JSON.parse(minify(fs.readFileSync(
                    configPath, { encoding: 'utf8' })))
    ;

if (usingDefaultConfig) {
    config.paths.push(path.join(__dirname, '../', 'data'));
}

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

var controller = new Controller(config);

process.nextTick(function() {
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

