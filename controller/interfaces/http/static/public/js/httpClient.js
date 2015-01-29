// Client side HTTP API exerciser for Greyhound.

(function(w) {
    'use strict';

    // get URL parameters
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

    var addBinaryAjaxTransport = function() {
        // Add AJAX transport for binary buffer.  From:
        // http://www.henryalgus.com/reading-binary-files-using-jquery-ajax/
        $.ajaxTransport(
            '+binary',
            function(options, originalOptions, jqXHR) {
                // check for conditions and support for blob / arraybuffer
                // response type.
                if (
                    window.FormData &&
                    ((options.dataType && (options.dataType == 'binary')) ||
                     (options.data &&
                        ((window.ArrayBuffer &&
                            options.data instanceof ArrayBuffer) ||
                         (window.Blob && options.data instanceof Blob)))))
                {
                    return {
                        // create new XMLHttpRequest
                        send: function(_, callback){
                            // setup all variables
                            var xhr = new XMLHttpRequest(),
                                url = options.url,
                                type = options.type,
                                // blob or arraybuffer. Default is blob
                                dataType = options.responseType || "blob",
                                data = options.data || null;

                            xhr.addEventListener('load', function(){
                                var data = {};
                                data[options.dataType] = xhr.response;
                                // make callback and send data
                                callback(
                                    xhr.status,
                                    xhr.statusText,
                                    data,
                                    xhr.getAllResponseHeaders());
                            });

                            xhr.open(type, url, true);
                            xhr.responseType = dataType;
                            xhr.send(data);
                        },
                        abort: function(){
                            jqXHR.abort();
                        }
                    };
                }
            }
        );
    }

    var schema = function() {
        return JSON.stringify(
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
            ]
        );
    }

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

    var downloadData = function(setStatus, cb) {
        var url = w.location.host;
        var match = w.location.pathname.match('\/http\/([^\/]+)');
        var pipelineId = match ? match[1] : null;

        if (!pipelineId) {
            return cb('No pipeline selected!');
        }

        var statsUrl = 'http://' + url + '/pipeline/' + pipelineId + '/stats';

        $.get(statsUrl, function(statsRes) {
            if (statsRes.status != 1) {
                return cb('STATS returned invalid status');
            }

            var stats = statsRes.stats;
            console.log(stats.stages['filters.stats']);

            var readUrl =
                'http://' + url + '/pipeline/' + pipelineId + '/read';

            var query = w.location.search;
            var sep = (query ? '&' : '?');
            setStatus("Read initiated, waiting for response...");

            $.ajax({
                dataType: 'binary',
                responseType: 'arraybuffer',
                type: 'GET',
                url: readUrl + w.location.search + sep + 'schema=' + schema()
            }).done(function(readRes, status, request) {
                var dataBuffer = new Int8Array(readRes);
                var numPoints  =
                    request.getResponseHeader('X-Greyhound-Num-Points');
                var rasterMeta = JSON.parse(
                    request.getResponseHeader('X-Greyhound-Raster-Meta'));

                return cb(null, dataBuffer, numPoints, rasterMeta, stats);
            }).fail(function(err) {
                console.log('READ failed');
                return cb('Failed' + err);
            });
        });
    }

    w.doIt = function() {
        addBinaryAjaxTransport();
		$("#stats").hide();
		downloadData(message, function(err, data, count, meta, stats) {
			if (err) {
				return errorOut(err.message);
            }

			console.log(
                'Got',
                data.byteLength,
                'bytes in',
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

