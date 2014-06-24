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
	Q = require('q');


var go = function() {
	// Set up Express app!
	var app = express();

	app.configure(function() {
		// all environments
		app.set('views', __dirname + '/views');
		app.set('view engine', 'jade');

		app.use(express.logger('dev'));
		app.use(express.bodyParser());
		app.use(express.methodOverride());

		app.use(express.cookieParser());
		var sessionStore = new express.session.MemoryStore();
		app.use(express.session({secret: 'windoge', store : sessionStore}));

		// Set the x-powered-by header
		app.use(function (req, res, next) {
			res.header("X-powered-by", "Hobu, Inc.");
			next();
		});

		app.use(require('less-middleware')({ src: __dirname + '/public', debug: true }));
		app.use(express.static(__dirname + '/public'));


		// development only
		if ('development' == app.get('env')) {
			app.use(express.errorHandler());
		}

		app.use(app.router);
	});

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
