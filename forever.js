var
    forever = require('forever-monitor'),
    argv = require('minimist')(process.argv.slice(2)),
    globalConfig = (require('./config').global || { }),
    quitForeverExitCode = (globalConfig.quitForeverExitCode || 42),
    relaunchAttempts = globalConfig.relaunchAttempts;

if (argv._.length != 2) {
    console.error('Usage: forever SOURCE_FILE COMPONENT');
    process.exit(1);
}

var filename = argv._[0];
process.title = argv._[1] + '_monitor';

var options = { killTree: true };

if (relaunchAttempts || relaunchAttempts === 0) {
    // If this is null or undefined in the configuration, do not pass it as an
    // option, because Forever.js will treat it as a zero, meaning the child
    // will never be relaunched.  We want to use 'null' or 'undefined' to mean
    // "restart eternally", so we must omit the 'max' option in this case.
    options.max = relaunchAttempts;
}

var child = new (forever.Monitor)(filename, options);

child.on('watch:restart', function(info) {
    console.log('Restarting script because ' + info.file + ' changed');
});

child.on('restart', function() {
    console.log('Restart instance number', child.times);
});

child.on('exit:code', function(code) {
    console.log('Forever detected script exited with code ' + code);

    if (code == quitForeverExitCode) {
        console.log('Child requesting permanent quit.  Monitor exiting.');
        process.exit(0);
    }
});

child.on('exit', function () {
    console.log(
        'Application exited permanently after',
        child.times,
        'failure(s)');
});

child.start();

