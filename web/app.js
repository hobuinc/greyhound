// app.js
// web front-end
//

process.title = 'gh_web';

var
	// node modules
	fs = require('fs'),
	http = require('http'),
	path = require('path'),

	// npm modules
	express = require('express'),
    methodOverride = require('method-override'),
    bodyParser = require('body-parser');


var go = function() {
	// Set up Express app!
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
    app.use(function (req, res, next) {
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

	app.get('/', function(req, res) {
        console.log('Query params: ', req.query);
        // This will fail due to no pipeline selection.
		res.render('index');
	});

    app.get('/data/:pipelineId', function(req, res) {
        console.log('Query params: ', req.query);
        res.render('data');
    });

	var server = http.createServer(app),
	port = process.env.PORT || 80;

	server.listen(port, function () {
		console.log('Web server running on port ' + port);
	});
};

process.nextTick(go)

