#!/usr/bin/env node

process.env.EXIT_ON_DONE = true;

var path = require('path');
var args = process.argv.slice(2);

var forever = require('forever-monitor');
var greyhound = new (forever.Monitor)(path.join(__dirname, 'app.js'), {
    args: args
});

greyhound.on('restart', function() {
    console.error('Relaunching greyhound');
});

greyhound.start();

