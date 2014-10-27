// app.js
// web front-end
//

process.title = 'gh_web';

var
	// node modules
	fs = require('fs'),
	http = require('http'),
	path = require('path'),
    disco = require('../common').disco,
    config = (require('../config').web || { }),
    globalConfig = (require('../config').global || { }),

	// npm modules
	express = require('express'),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser'),
    console = require('clim')();

var go = function() {
	var app = express();

    app.set('views', __dirname + '/views');
    app.set('view engine', 'jade');

    app.use(express.logger('dev'));
    app.use(bodyParser.json());
    app.use(bodyParser.urlencoded({ extended: true }));
    app.use(methodOverride());

    app.use(express.cookieParser());
    var sessionStore = new express.session.MemoryStore();
    app.use(express.session({ secret: 'windoge', store : sessionStore }));

    // Set the x-powered-by header
    app.use(function(req, res, next) {
        res.header("X-powered-by", "Hobu, Inc.");
        next();
    });

    app.use(require('less-middleware')(path.join(__dirname, 'public')));
    app.use(express.static(__dirname + '/public'));

    // development only
    if ('development' == app.get('env')) {
        app.use(express.errorHandler());
    }

    app.use(app.router);

	app.get('/(greyhound)?', function(req, res) {
        console.log('Query params: ', req.query);
        // This will fail due to no pipeline selection.
		res.render('index');
	});

    app.get('(/greyhound)?/data/:pipelineId', function(req, res) {
        console.log('Query params: ', req.query);
        res.render('data');
    });

	var server = http.createServer(app),
	port = config.port || 8080;

    // Register ourselves with disco for status purposes.
    disco.register('web', port, function(err, service) {
        if (err) return console.log("Failed to register service:", err);

        server.listen(service.port, function () {
            console.log('Web server running on port ' + port);
        });
    });
};

if (config.enable !== false) {
    process.nextTick(go);
}
else {
    process.exit(globalConfig.quitForeverExitCode || 42);
}

