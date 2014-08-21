// Unit test suite to exercise Greyhound from a websocket client.

var fs = require('fs');
var WebSocket = require('ws');
var ws, timeoutObj;
var timeoutMs = 15000;
var samplePipelineId = '58a6ee2c990ba94db936d56bd42aa703';
var samplePoints = 10653;
var sampleBytes = 340896;
var sampleStride = 32;
var sampleSrs = 'PROJCS["NAD_1983_HARN_Lambert_Conformal_Conic",GEOGCS["GCS_North_American_1983_HARN",DATUM["NAD83_High_Accuracy_Regional_Network",SPHEROID["GRS_1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6152"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",43],PARAMETER["standard_parallel_2",45.5],PARAMETER["latitude_of_origin",41.75],PARAMETER["central_meridian",-120.5],PARAMETER["false_easting",1312335.958005249],PARAMETER["false_northing",0],UNIT["foot",0.3048,AUTHORITY["EPSG","9002"]]]';

var bigPipelineId = '3c51e54a3f0e1b7f4ffd582d4d970162';

var send = function(obj) {
    ws.send(JSON.stringify(obj));
}

var setInitialCmd = function(obj) {
    ws.on('open', function() {
        send(obj);
    });
}

var isArray = function(obj) {
    return Object.prototype.toString.call(obj) === '[object Array]';
}

var doExchangeSet = function(test, exchangeSet) {
    timeoutObj = startTestTimer(test);

    var exchangeIndex = 0;
    var responses = [];
    setInitialCmd(exchangeSet[exchangeIndex]['req']);
    var expected = {};
    var burst = false;

    ws.on('message', (function(data, flags) {
        // If the expected value is an array instead of a verification
        // object, then we expect to receive:
        //      1) An initial JSON response, followed by
        //      2) Some number of binary responses.
        if (!burst)
        {
            expected = exchangeSet[exchangeIndex]['res'];
            burst = isArray(expected);
            if (burst)
                expected = expected[0];
        }

        // Validate response.
        if (!flags.binary) {
            if (typeof(expected) !== 'function') {
                var json = JSON.parse(data);
                responses[exchangeIndex] = json;
                validateJson(test, json, expected, exchangeIndex);

                if (burst)
                {
                    // Move on to the binary validation function.
                    expected = exchangeSet[exchangeIndex]['res'][1];
                }
            }
            else {
                burst = !expected({}, {}, JSON.parse(data));
            }
        }
        else {
            if (typeof(expected) === 'function') {
                // The burst is over when the function returns true, at which
                // point we can increment into the next exchange index.
                burst = !expected(data, responses);
            }
            else {
                var message = 'Got unexpected binary response';
                if (exchangeSet[exchangeIndex]['req']
                        .hasOwnProperty('command')) {
                    message += ' to: ' +
                        exchangeSet[exchangeIndex]['req']['command'];
                }

                test.ok(false, message);
                endTest(test);
            }
        }

        if (!burst)
        {
            // Send request for the next exchange.
            if (++exchangeIndex < exchangeSet.length) {
                var rawReq = exchangeSet[exchangeIndex]['req'];
                var req = {};

                for (var field in rawReq) {
                    if (typeof rawReq[field] !== 'function') {
                        req[field] = rawReq[field];
                    }
                    else {
                        req[field] = rawReq[field](responses);
                    }
                }

                send(req);
            }
            else {
                endTest(test);
            }
        }
    }));
}

var endTest = function(test) {
    clearTimeout(timeoutObj);
    test.done();
}

var startTestTimer = function(test) {
    return setTimeout(function() {
        test.ok(false, 'Test timed out!');
        test.done();
    },
    timeoutMs);
}

var dontCare = function() {
    return true;
}

var ghSuccess = function(rxStatus) {
    return rxStatus === 1;
}

var ghFail = function(rxStatus) {
    return !ghSuccess(rxStatus);
}

var initialSession = function(prevResponses) {
    return prevResponses[0]['session'];
}

var validateJson = function(test, json, expected, exchangeIndex) {
    for (var field in expected) {
        test.ok(
            json.hasOwnProperty(field),
            'Missing property ' + field);

        if (json.hasOwnProperty(field)) {
            if (typeof expected[field] !== "function") {
                test.ok(
                    json[field] === expected[field],
                    'Expected json[' + field + '] === ' + expected[field] +
                            ', got: ' + json[field]);
            }
            else {
                test.ok(
                    expected[field](json[field]),
                    'Validation function failed for "' + field +
                    '", parameter was: ' + json[field]);
            }
        }
    }

    for (var field in json)
    {
        test.ok(
            expected.hasOwnProperty(field) ||
                    field === 'reason' ||
                    field === 'message',
            'Unexpected field in response #' + (exchangeIndex + 1) + ': ' +
                    field + " - " + json[field]);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Contents:
//      PUT
//      CREATE
//      POINTSCOUNT
//      DIMENSIONS
//      SRS
//      READ
//      CANCEL
//      DESTROY
//      OTHER
//
//////////////////////////////////////////////////////////////////////////////

module.exports = {
    setUp: function(cb) {
        ws = new WebSocket('ws://localhost:' + (process.env.PORT || 80) + '/');
        cb();
    },

    tearDown: function(cb) {
        ws.close();
        cb();
    },

    // PUT - test with empty pipeline
    // Expect: failure status
    testPutEmptyPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'pipeline': '',
                },
                res: {
                    'command':  'put',
                    'status':   ghFail,
                }
            }]
        );
    },

    // PUT - test with non-string pipeline
    // Expect: failure status
    testPutWrongTypePipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'pipeline': 42,
                },
                res: {
                    'command':  'put',
                    'status':   ghFail,
                }
            }]
        );
    },

    // PUT - test with non-string pipeline
    // Expect: failure status
    testPutFunctionPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'pipeline': function() { console.log('Wrong'); },
                },
                res: {
                    'command':  'put',
                    'status':   ghFail,
                }
            }]
        );
    },

    // PUT - test with malformed pipeline XML
    // Expect: failure status
    testPutMalformedPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'pipeline': 'I am not valid pipeline XML!',
                },
                res: {
                    'command':  'put',
                    'status':   ghFail,
                }
            }]
        );
    },

    // PUT - test with missing pipeline parameter
    // Expect: failure status
    testPutMissingPipelineParam: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                },
                res: {
                    'command':  'put',
                    'status':   ghFail,
                }
            }]
        );
    },
    // PUT - test double call with the same pipeline (this also tests
    // the nominal case)
    // Expect: Two successful statuses with a pipelineId parameter in each
    // response
    testPutDoublePipeline: function(test) {
        var filename = '/vagrant/examples/data/read.xml';
        file = fs.readFileSync(filename, 'utf8');

        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'pipeline': file,
                },
                res: {
                    'command':      'put',
                    'status':       ghSuccess,
                    'pipelineId':   dontCare,
                }
            },
            {
                req: {
                    'command':  'put',
                    'pipeline': file,
                },
                res: {
                    'command':      'put',
                    'status':       ghSuccess,
                    'pipelineId':   dontCare,
                }
            }]
        );
    },

    // CREATE - test without a pipelineId parameter
    // Expect: failure status
    testCreateNoPipelineId: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'create',
                },
                res: {
                    'command':  'create',
                    'status':   ghFail,
                },
            }]
        );
    },

    // CREATE - test with an invalid pipeline ID
    // Expect: failure status
    testCreateInvalidPipelineId: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   'This is not a valid pipelineId',
                },
                res: {
                    'command':      'create',
                    'status':       ghFail,
                },
            }]
        );
    },

    // CREATE - test with a non-string pipeline ID
    // Expect: failure status
    testCreateWrongTypePipelineId: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   42,
                },
                res: {
                    'command':      'create',
                    'status':       ghFail,
                },
            }]
        );
    },

    // CREATE - test with a function as the pipeline ID
    // Expect: failure status
    testCreateFunctionPipelineId: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   function() { console.log('Wrong'); },
                },
                res: {
                    'command':      'create',
                    'status':       ghFail,
                },
            }]
        );
    },

    // CREATE - test with a wildcard pipeline ID
    // Expect: failure status
    testCreateWildcardPipelineId: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   '*',
                },
                res: {
                    'command':      'create',
                    'status':       ghFail,
                },
            }]
        );
    },

    // CREATE - test valid command
    // Expect: successful status and 'session' parameter in response
    testCreateValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'create',
                    'pipelineId': samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command': 'destroy',
                    'session': function(prevResponses) {
                        var prev = prevResponses[prevResponses.length - 1];
                        return prev['session'];
                    },
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // CREATE - test multiple sessions created with the same pipeline
    // Expect: two successful statuses with different 'session' parameters
    testCreateDouble: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  function(prevResponses) {
                        // Destroy the second session created.
                        var prev = prevResponses[prevResponses.length - 2];
                        return prev['session'];
                    },
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // POINTSCOUNT - test command with missing 'session' parameter
    // Expect: failure status
    testPointsCountMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'pointsCount',
                },
                res: {
                    'command':  'pointsCount',
                    'status':   ghFail,
                },
            }]
        );
    },

    // POINTSCOUNT - test command with invalid 'session' parameter
    // Expect: failure status
    testPointsCountInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'pointsCount',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'pointsCount',
                    'status':   ghFail,
                },
            }]
        );
    },

    // POINTSCOUNT - test command with object 'session' parameter
    // Expect: failure status
    testPointsCountObjectSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'pointsCount',
                    'session':  { session: 'I am an invalid session object!' }
                },
                res: {
                    'command':  'pointsCount',
                    'status':   ghFail,
                },
            }]
        );
    },

    // POINTSCOUNT - test command with a function as the 'session' parameter
    // Expect: failure status
    testPointsCountFunctionSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'pointsCount',
                    'session':  function() { console.log('Wrong'); },
                },
                res: {
                    'command':  'pointsCount',
                    'status':   ghFail,
                },
            }]
        );
    },

    // POINTSCOUNT - test valid command
    // Expect: successful status and number of points
    testPointsCountValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'pointsCount',
                    'session':  initialSession,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'pointsCount',
                    'count':    10653,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // DIMENSIONS - test command with missing 'session' parameter
    // Expect: failure status
    testDimensionsMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'dimensions',
                },
                res: {
                    'command':  'dimensions',
                    'status':   ghFail,
                },
            }]
        );
    },

    // DIMENSIONS - test command with invalid 'session' parameter
    // Expect: failure status
    testDimensionsInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'dimensions',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'dimensions',
                    'status':   ghFail,
                },
            }]
        );
    },

    // DIMENSIONS - test valid command
    // Expect: Successful status and dimensions list
    testDimensionsValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'dimensions',
                    'session':  initialSession,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'dimensions',
                    'dimensions':   dontCare,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // SRS - test command with missing 'session' parameter
    testSrsMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'srs',
                },
                res: {
                    'command':  'srs',
                    'status':   ghFail,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // SRS - test command with invalid 'session' parameter
    testSrsInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'srs',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'srs',
                    'status':   ghFail,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // SRS - test valid command
    testSrsValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'srs',
                    'session':  initialSession,
                },
                res: {
                    'command':  'srs',
                    'status':   ghSuccess,
                    'srs':      sampleSrs,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test command with missing 'session' parameter
    // Expect: failure status
    testReadMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'read',
                },
                res: {
                    'command':  'read',
                    'status':   ghFail,
                },
            }]
        );
    },

    // READ - test command with invalid 'session' parameter
    // Expect: failure status
    testReadInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'read',
                    'status':   ghFail,
                },
            }]
        );
    },

    // READ - test request of zero points
    // Expect: all points read
    testReadZeroPoints: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test multiple sessions with interleaved read requests
    // Expect: all points read for both sessions, successful destroys
    testReadMultipleSessions: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        if (bytesRead === sampleBytes) {
                            // Reset for the next read.
                            bytesRead = 0;
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                ]
            },
            {
                req: {
                    'command':  'read',
                    'session':  function(prevResponses) {
                        // Issue a read on the second session created.
                        var prev = prevResponses[1];
                        return prev['session'];
                    },
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  function(prevResponses) {
                        // Issue a destroy on the second session created.
                        var prev = prevResponses[1];
                        return prev['session'];
                    },
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test string parameter for numPoints
    // Expect: failure status - nothing read
    testReadStringNumPoints: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    'Wrong type!',
                },
                res:
                {
                    'status':       ghFail,
                    'command':      'read',
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test negative number of points requested
    // Expect: all points read
    testReadNegativeNumPoints: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    -2,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test request of more points than exist in the pipeline
    // Expect: all points read
    testReadTooManyPoints: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    samplePoints + 50,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test request of offset >= numPoints
    // Expect: failure status, no points read - session still available
    testReadTooLargeOffset: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    1,
                    'start':    samplePoints,
                },
                res: {
                    'status':       ghFail,
                    'command':      'read',
                }
            },
            {
                // Now make sure a valid read still works.
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    10,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    10,
                        'numBytes':     10 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 10 * sampleStride;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test negative offset requested
    // Expect: successful read from offset = 0
    testReadNegativeOffset: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    10,
                    'start':    -1,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    10,
                        'numBytes':     10 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 10 * sampleStride;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test get complete buffer
    // Expect: all points read
    testReadAll: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test with non-zero count and offset
    // Expect: proper number of bytes read
    testReadCountAndOffset: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    20,
                    'start':    30,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    20,
                        'numBytes':     20 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 20 * sampleStride;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test missing offset
    // Expect: proper number of bytes read from start
    testReadNoOffsetSupplied: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    20,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    20,
                        'numBytes':     20 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 20 * sampleStride;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test missing count
    // Expect: Successful read from supplied offset until the end
    testReadNoCountSupplied: function(test) {
        var bytesRead = 0;
        var suppliedOffset = samplePoints - 10;
        var expectedBytes = (samplePoints - suppliedOffset) * sampleStride;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'start':    suppliedOffset,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints - suppliedOffset,
                        'numBytes':     expectedBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === expectedBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test that multiple reads may be issued on the same session.
    // Expect: both reads complete successfully
    testDoubleRead: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        if (bytesRead === sampleBytes) {
                            // Reset the counter for the next read.
                            bytesRead = 0;
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                ]
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // READ - test idle cancel (i.e. no ongoing read) followed by read
    // Expect: cancel command succeeds (but useless), then all points read
    testIdleCancel: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'cancel',
                    'session':  initialSession,
                },
                res: {
                    'command':      'cancel',
                    'status':       ghSuccess,
                    'cancelled':    false,
                }
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // TODO Test READ with point/radius query
    // TODO Test READ with quadtree indexed queries

    // CANCEL - test cancel functionality and subsequent read
    // Expect: Partially transmitted data, successful cancel, successful read
    testValidCancel: function(test) {
        console.log('Starting long test (~10 seconds)...');
        var bytesRead = 0;
        var bytesExpected = 7954265 * sampleStride;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   bigPipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    0,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    dontCare,
                        'numBytes':     dontCare,
                    },
                    function(data, prevResponses, json) {
                        if (json) {
                            // At some point we'll get a reply to our cancel.
                            test.ok(json['command'] === 'cancel');
                            test.ok(ghSuccess(json['status']));
                            test.ok(json['cancelled'] === true);
                            test.ok(json.hasOwnProperty('numBytes'));

                            // This reply will give us a new number of expected
                            // bytes, so overwrite our expected number we got
                            // from the response to 'read'.
                            bytesExpected = json['numBytes'];
                        }
                        else {
                            // On the first binary blob, send the cancel.
                            if (bytesRead === 0) {
                                send({
                                    'command': 'cancel',
                                    'session': prevResponses[0].session,
                                });
                            }

                            // Consume all binary blobs.
                            bytesRead += data.length;
                        }

                        if (bytesRead === bytesExpected) {
                            // Reset this for the next test read.
                            bytesRead = 0;
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                ]
            },
            {
                req: {
                    'command':  'read',
                    'session':  initialSession,
                    'count':    20,
                    'start':    30,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'numPoints':    20,
                        'numBytes':     20 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 20 * sampleStride;
                    }
                ]
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            }]
        );
    },

    // DESTROY - test command with missing 'session' parameter
    // Expect: failure status
    testDestroyMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'destroy',
                },
                res: {
                    'command':  'destroy',
                    'status':   ghFail,
                },
            }]
        );
    },

    // DESTROY - test command with invalid 'session' parameter
    // Expect: failure status
    testDestroyInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'destroy',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'destroy',
                    'status':   ghFail,
                },
            }]
        );
    },

    // DESTROY - test valid destroy
    // Expect: Successful destroy, session not useable or re-destroyable after
    // initial destroy
    testDestroyValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':      'create',
                    'pipelineId':   samplePipelineId,
                },
                res: {
                    'command':  'create',
                    'status':   ghSuccess,
                    'session':  dontCare,
                },
            },
            {
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghSuccess,
                },
            },
            {
                // Try to use session again - should not work
                req: {
                    'command':  'pointsCount',
                    'session':  initialSession,
                },
                res: {
                    'status':   ghFail,
                    'command':  'pointsCount',
                },
            },
            {
                // Try to destroy again - should not work
                req: {
                    'command':  'destroy',
                    'session':  initialSession,
                },
                res: {
                    'command':  'destroy',
                    'status':   ghFail,
                },
            },
            ]
        );
    },

    // OTHER - test non-existent command
    testOtherBadCommand: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'badCommand',
                },
                res: {
                    'status':   ghFail,
                },
            }]
        );
    },

    // OTHER - test missing 'command' parameter
    testOtherMissingCommand: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                },
                res: {
                    'status':   ghFail,
                },
            }]
        );
    },

    // OTHER - test empty 'command' parameter
    testOtherEmptyCommand: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  '',
                },
                res: {
                    'status':   ghFail,
                },
            }]
        );
    },
};

