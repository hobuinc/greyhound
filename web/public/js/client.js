// client.js
// Client side stuffs for greyhound web viewer
//

(function(w) {
	"use strict";

    // get URL parameters
    // for now, using an adaptation from: http://stackoverflow.com/a/2880929
    var getUrlParameters = function(query) {
        query = query.substring(1);

        var match,
            pl     = /\+/g, // Replace '+' with a space.
            search = /([^&=]+)=?([^&]*)/g,
            decode = function(s) {
                return decodeURIComponent(s.replace(pl, " "));
            },
            urlParams = {};

        while (match = search.exec(query))
           urlParams[decode(match[1])] = decode(match[2]);

        return urlParams;
    };

	// show an error message to the user
	//
	var errorOut = function(msg) {
		$("#messages").html("<p class='error'>" + msg + "</p>");
		console.log('Error : ' + msg);
	};

	// show a status message to the user
	var message = function(msg) {
		$("#messages").html("<p class='message'>" + msg + "</p>");
		console.log('Status: ' + msg);
	};

	// download data over from the server and call the cb when done
	//
	var downloadData = function(status_cb, cb) {
		if (!w.WebSocket)
			return cb(new Error(
                    "Your browser doesn't seem to support websockets"));

		status_cb("Loading Point Cloud Data... Please Wait.");

		// prepare websocket URL and try to create a websocket
		//
		var wsURI = "ws://" + w.location.host + "/";
		var ws = new w.WebSocket(wsURI);

		// get data as array buffer
		ws.binaryType = "arraybuffer";

		// Websocket open handler
		// Send a command to create a session, we will initiate an actual read
		// when we get confirmation that the connection was created
		ws.onopen = function() {
			status_cb("WebSocket connection established. Creating session...");

            // var urlParams = getUrlParameters(w.location.search);

            var match = w.location.pathname.match('\/data\/([^\/]+)');

            if (match)
            {
                ws.send(JSON.stringify({
                    command: 'create',
                    pipelineId: match[1],
                }));
            }
            else
            {
                status_cb("No pipeline selected!");
            }
		};

		// General handler for all messages, our logic goes in here
		//
		var count;
		var session;
		var pointsCount;
		var dataBuffer = null; // buffer to collect recieved data
        var meta = null;
        var stats = null;

		ws.onmessage = function(evt) {
			if (typeof(evt.data) === "string") {
				var msg = JSON.parse(evt.data);
				console.log('Incoming:', msg);

				if (msg.command === "create") {
                    session	= msg.session;

                    ws.send(JSON.stringify({
                        command: 'stats',
                        session: msg.session
                    }));
                }
                else if (msg.command === "stats") {
                    stats = JSON.parse(msg.stats);
                    console.log(stats.stages['filters.stats']);

					if (msg.status === 0)
						return cb(new Error(
                                'Failed to create session, this is not good.'));

                    var readParams = { command: 'read', session: session };
                    var urlParams = getUrlParameters(w.location.search);

                    /*
                    // TODO Parses standard '+' separated query string.  Will
                    // this be used for any queries, or all GeoJson?
                    if (urlParams.hasOwnProperty('point') &&
                        urlParams.hasOwnProperty('radius')) {
                        var point = urlParams['point'].split(" ");

                        if (point.length >= 2) {
                            readParams['x'] = parseFloat(point[0]);
                            readParams['y'] = parseFloat(point[1]);

                            if (point.length == 3)
                                readParams['z'] = parseFloat(point[2]);

                            readParams['radius'] =
                                parseFloat(urlParams['radius']);
                        }
                    }
                    */

                    if (urlParams.hasOwnProperty('geo')) {
                        var geo = jQuery.parseJSON(urlParams['geo']);

                        if (geo['type'] === 'Point') {
                            var coords = geo['coordinates'];

                            readParams['x'] = coords[0];
                            readParams['y'] = coords[1];

                            if (coords.length === 3)
                                readParams['z'] = coords[2];

                            readParams['radius'] =
                                parseFloat(urlParams['radius']);
                        }

                        if (geo.hasOwnProperty('bbox'))
                            readParams['bbox'] = geo['bbox'];
                    }

                    if (urlParams.hasOwnProperty('resolution')) {
                        var resolution =
                            jQuery.parseJSON(urlParams['resolution']);

                        if (resolution.length == 2) {
                            readParams['resolution'] = resolution;
                        }
                    }

                    readParams['schema'] =
                    [
                        {
                            "name": "X",
                            "type": "floating",
                            "size": "4"
                        },
                        {
                            "name": "Y",
                            "type": "floating",
                            "size": "4"
                        },
                        {
                            "name": "Z",
                            "type": "floating",
                            "size": "4"
                        },
                        {
                            "name": "Intensity",
                            "type": "unsigned",
                            "size": "2"
                        },
                        {
                            "name": "Red",
                            "type": "unsigned",
                            "size": "2"
                        },
                        {
                            "name": "Green",
                            "type": "unsigned",
                            "size": "2"
                        },
                        {
                            "name": "Blue",
                            "type": "unsigned",
                            "size": "2"
                        },
                    ];

                    if (urlParams.hasOwnProperty('depthBegin'))
                        readParams['depthBegin'] = urlParams['depthBegin'];

                    if (urlParams.hasOwnProperty('depthEnd'))
                        readParams['depthEnd'] = urlParams['depthEnd'];

                    if (urlParams.hasOwnProperty('rasterize'))
                        readParams['rasterize'] = urlParams['rasterize'];

					// This is in response to our create request.  Now request
                    // to receive the data.
					ws.send(JSON.stringify(readParams));

					status_cb("Read initiated, waiting for response...");
				}
				else if (msg.command === "read") {
					if (msg.status !== 1)
						return cb(new Error(
                                    "Failed to queue read request: " +
                                    (msg.reason || "Unspecified error")));

					status_cb("Reading data... Please wait.");

					console.log(msg.numPoints, msg.numBytes);

					count		= 0;
					pointsCount	= msg.numPoints;
					dataBuffer	= new Int8Array(msg.numBytes);

                    if (msg.hasOwnProperty('rasterMeta'))
                    {
                        meta = msg.rasterMeta;
                    }

                    if (pointsCount == 0) {
                        // we're done reading data, close connection
                        ws.send(JSON.stringify({
                            command: 'destroy',
                            session: session
                        }));
                    }
				}
				else if (msg.command === "destroy") {
					if (msg.status === 1) {
						ws.close();
						return status_cb("Data read successfully.");
					}

					cb(new Error("Session destroy command failed"));
				}
			}
			else {
				var a = new Int8Array(evt.data);
				dataBuffer.set(a, count);

				count += a.length;

				if (count >= dataBuffer.byteLength) {
					// we're done reading data, close connection
					ws.send(JSON.stringify({
						command: 'destroy',
						session: session
					}));
				}
			}
		};

		// close and cleanup data
		ws.onclose = function() {
			if(dataBuffer !== null) {
                // Use setTimeout so that we call the callback outside the
                // context of ws.onclose
				setTimeout(function() {
					cb(null, dataBuffer, pointsCount, meta, stats);
				}, 0);
			}
		};
	}

	w.doIt = function() {
		$("#stats").hide();
		downloadData(message, function(err, data, count, meta, stats) {
			if (err)
				return errorOut(err.message);

			console.log(
                'Got',
                data.byteLength,
                'total bytes in',
                count,
                'points');

			message("Data download complete, handing over to renderer.");
			try {
				renderPoints(data, count, meta, stats, message);
			}
			catch(e) {
				errorOut(e.message);
			}
		});
	};
})(window);

$(function() {
	doIt();
});

