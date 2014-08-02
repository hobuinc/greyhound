// Unit test suite to exercise Greyhound from a websocket client.

var fs = require('fs');
var WebSocket = require('ws');
var ws, timeoutObj;
var timeoutMs = 1000;
var samplePipelineId = 'd4f4cc08e63242a201de6132e5f54b08';
var samplePoints = 10653;
var sampleBytes = 213060;
var sampleStride = 20;

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
        else {
            if (typeof(expected) === 'function') {
                // The burst is over when the function returns true, at which
                // point we can increment into the next exchange index.
                burst = !expected(data);
            }
            else {
                var message = 'Got unexpected non-binary response';
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
//      SCHEMA
//      SRS
//      READ
//      DESTROY
//      OTHER
//
//////////////////////////////////////////////////////////////////////////////

// TODO Query sanitization
// TODO Invalid types testing (strings for int fields, floats for ints, etc.)

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

    // PUT - test with malformed pipeline XML
    // Expect: failure status
    testPutMalformedPipeline: function(test) {
        /*
        // TODO Server needs to validate pipelines via PDAL
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
        */

        console.log('This test is a TODO');
        test.done();
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
                    'command': 'create',
                    'pipelineId': 'This is not a valid pipelineId',
                },
                res: {
                    'command':  'create',
                    'status':   ghFail,
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

    // SCHEMA - test command with missing 'session' parameter
    // Expect: failure status
    testSchemaMissingSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'schema',
                },
                res: {
                    'command':  'schema',
                    'status':   ghFail,
                },
            }]
        );
    },

    // SCHEMA - test command with invalid 'session' parameter
    // Expect: failure status
    testSchemaInvalidSession: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'schema',
                    'session':  'I am an invalid session string!',
                },
                res: {
                    'command':  'schema',
                    'status':   ghFail,
                },
            }]
        );
    },

    // SCHEMA - test valid command
    // Expect: Successful status and schema
    testSchemaValid: function(test) {
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
                    'command':  'schema',
                    'session':  initialSession,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'schema',
                    'schema':   dontCare,
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
        // TODO
        test.done();
    },

    // SRS - test command with invalid 'session' parameter
    testSrsInvalidSession: function(test) {
        // TODO
        test.done();
    },

    // SRS - test valid command
    testSrsValid: function(test) {
        // TODO
        test.done();
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
    testDoubleRead: function(test) {
        // TODO
        console.log('This test is a TODO');
        test.done();
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

