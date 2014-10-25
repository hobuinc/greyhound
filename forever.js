var
    forever = require('forever-monitor'),
    argv = require('minimist')(process.argv.slice(2)),
    globalConfig = (require('./config').global || { }),
    quitForeverExitCode = (globalConfig.quitForeverExitCode || 42);

if (argv._.length != 2) {
    console.error('Usage: forever SOURCE_FILE COMPONENT');
    process.exit(1);
}

var filename = argv._[0];
process.title = argv._[1] + '_monitor';

var child = new (forever.Monitor)(filename, {
    'killTree': true
});

child.on('watch:restart', function(info) {
    console.log('Restarting script because ' + info.file + ' changed');
});

child.on('restart', function() {
    console.log('Forever restarting script for ' + child.times + ' time');
});

child.on('exit:code', function(code) {
    console.log('Forever detected script exited with code ' + code);

    if (code == quitForeverExitCode) {
        console.log('Child requesting permanent quit.  Monitor exiting.');
        process.exit(0);
    }
});

child.on('exit', function () {
    console.log('Application exited permanently');
});

child.start();

