// Client side HTTP API exerciser for Greyhound.

(function(w) {
    'use strict';

    var getQueryParam = function(name) {
        name = name.replace(/[\[\]]/g, "\\$&");
        var regex = new RegExp("[?&]" + name + "(=([^&#]*)|&|#|$)"),
            results = regex.exec(window.location.href);
        if (!results) return null;
        if (!results[2]) return '';
        return decodeURIComponent(results[2].replace(/\+/g, " "));
    };

    // get URL parameters
    var getUrlParameters = function(query) {
        query = query || w.location.search;
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
        console.log('pathname', w.location.pathname);
        var match = w.location.pathname.match('\/resource\/(.+)/static');
        console.log('match', match);
        var resourceId = match ? match[1] : null;
        if (!resourceId) resourceId = getQueryParam('r');
        if (!resourceId) return cb('No resource selected!');

        var readUrl = '//' + url + '/resource/' + resourceId + '/read';

        var query = getUrlParameters() || { };
        delete query.r;
        query = Object.keys(query).reduce((p, c) => {
            if (c == 'compress' || c == 'schema') return p;
            return p + (p.length ? '&' : '?') + c + '=' + query[c];
        }, '');

        query = query + (query.length ? '&' : '?') + 'schema=' + schema();
        setStatus("Read initiated, waiting for response...");

        $.ajax({
            dataType: 'binary',
            responseType: 'arraybuffer',
            type: 'GET',
            url: readUrl + query
        }).done(function(readRes, status, request) {
            var dataBuffer = new Int8Array(readRes);

            var numPointsBuffer = new ArrayBuffer(4);
            var numPointsView = new DataView(numPointsBuffer);
            for (var i = 0; i < 4; ++i) {
                numPointsView.setUint8(
                    i,
                    dataBuffer[dataBuffer.length - i - 1]);
            }

            var numPoints = numPointsView.getUint32(0);
            return cb(null, dataBuffer, numPoints);
        }).fail(function(err) {
            console.log('READ failed');
            return cb('Failed' + err);
        });
    }

    w.doIt = function() {
        addBinaryAjaxTransport();
		$("#stats").hide();
		downloadData(message, function(err, data, count, meta) {
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
				renderPoints(data, count, meta, message);
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

