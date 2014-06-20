// app.js
// web front-end
//

var
	// node modules
	fs = require('fs'),
	http = require('http'),
	path = require('path'),

	// npm modules
	express = require('express'),
	_ = require('lodash'),
	Q = require('q'),
    logger = require('morgan'),
    bodyParser = require('body-parser'),
    methodOverride = require('method-override'),
    cookieParser = require('cookie-parser'),
    expressSession = require('express-session');
    errorHandler = require('errorhandler');


var go = function() {
	// Set up Express app!
	var app = express();

    // all environments
    app.set('views', __dirname + '/views');
    app.set('view engine', 'jade');

    app.use(logger);
    app.use(bodyParser);
    app.use(methodOverride);
    app.use(cookieParser);

    var sessionStore = new expressSession.MemoryStore();
    app.use(expressSession({secret: 'windoge', store : sessionStore}));

    // Set the x-powered-by header
    app.use(function (req, res, next) {
        res.header("X-powered-by", "Hobu, Inc.");
        next();
    });

    app.use(require('less-middleware')(__dirname + '/public', {debug: true}));
    app.use(express.static(__dirname + '/public'));


    // development only
    if ( 'development' == app.get('env') ) {
        app.use(errorHandler());
    }

	app.get('/', function(req, res) {
		res.render('index');
	});

	var server = http.createServer(app),
	port = process.env.PORT || 80;

	server.listen(port, function () {
		console.log('Web server running on port ' + port);
	});
};

process.nextTick(go);

