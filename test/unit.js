// Unit test suite to exercise Greyhound from a websocket client.

var fs = require('fs');
var WebSocket = require('ws');
var _ = require('lodash');
var ws, timeoutObj;
var timeoutMs = 20000;
var samplePipelineId = '5adcf597e3376f98471bf37816e9af2c';
var samplePoints = 10653;
var sampleBytes = 213060;
var sampleStride = 20;
var sampleSrs = 'PROJCS["NAD_1983_HARN_Lambert_Conformal_Conic",GEOGCS["GCS_North_American_1983_HARN",DATUM["NAD83_High_Accuracy_Regional_Network",SPHEROID["GRS_1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6152"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",43],PARAMETER["standard_parallel_2",45.5],PARAMETER["latitude_of_origin",41.75],PARAMETER["central_meridian",-120.5],PARAMETER["false_easting",1312335.958005249],PARAMETER["false_northing",0],UNIT["foot",0.3048,AUTHORITY["EPSG","9002"]]]';

var bigPipelineId = 'a87d0a50e03a880c75e9f872c925f984';

// Request only attributes applicable for rendering the data visually.
var rendererSchema =
[
    {
        "name": "X",
        "type": "floating",
        "size": 4
    },
    {
        "name": "Y",
        "type": "floating",
        "size": 4
    },
    {
        "name": "Z",
        "type": "floating",
        "size": 4
    },
    {
        "name": "Intensity",
        "type": "unsigned",
        "size": 2
    },
    {
        "name": "Red",
        "type": "unsigned",
        "size": 2
    },
    {
        "name": "Green",
        "type": "unsigned",
        "size": 2
    },
    {
        "name": "Blue",
        "type": "unsigned",
        "size": 2
    },
    {
        // Invalid dimension in schema request - will always be omitted.
        "name": "BAD DIMENSION NAME",
        "type": "unsigned",
        "size": 2
    },
];

var rxSchema =
[
    {
        "name" : "X",
        "size" : 8,
        "type" : "floating"
    },
    {
        "name" : "Y",
        "size" : 8,
        "type" : "floating"
    },
    {
        "name" : "Z",
        "size" : 8,
        "type" : "floating"
    },
    {
        "name" : "GpsTime",
        "size" : 8,
        "type" : "floating"
    },
    {
        "name" : "Intensity",
        "size" : 2,
        "type" : "unsigned"
    },
    {
        "name" : "PointSourceId",
        "size" : 2,
        "type" : "unsigned"
    },
    {
        "name" : "Red",
        "size" : 2,
        "type" : "unsigned"
    },
    {
        "name" : "Green",
        "size" : 2,
        "type" : "unsigned"
    },
    {
        "name" : "Blue",
        "size" : 2,
        "type" : "unsigned"
    },
    {
        "name" : "ReturnNumber",
        "size" : 1,
        "type" : "unsigned"
    },
    {
        "name" : "NumberOfReturns",
        "size" : 1,
        "type" : "unsigned"
    },
    {
        "name" : "ScanDirectionFlag",
        "size" : 1,
        "type" : "unsigned"
    },
    {
        "name" : "EdgeOfFlightLine",
        "size" : 1,
        "type" : "unsigned"
    },
    {
        "name" : "Classification",
        "size" : 1,
        "type" : "unsigned"
    },
    {
        "name" : "ScanAngleRank",
        "size" : 1,
        "type" : "signed"
    },
    {
        "name" : "UserData",
        "size" : 1,
        "type" : "unsigned"
    }
];

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

                if (exchangeSet[exchangeIndex]['res'].length > 2)
                {
                    validateJson(
                            test,
                            JSON.parse(data),
                            exchangeSet[exchangeIndex]['res'][2],
                            exchangeIndex);

                    burst = false;
                }
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

var validateJson = function(test, json, expected, exchangeIndex) {
    for (var field in expected) {
        test.ok(
            json.hasOwnProperty(field),
            'Missing property ' + field);

        if (json.hasOwnProperty(field)) {
            if (typeof expected[field] !== "function") {
                test.ok(
                    _.isEqual(json[field], expected[field]),
                    'Expected json[' + field + '] ===\n' +
                        JSON.stringify(expected[field]) +
                        '\n\nGot:\n' + JSON.stringify(json[field]));
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
//      NUMPOINTS
//      SCHEMA
//      STATS
//      SRS
//      READ
//      CANCEL
//      DESTROY
//      OTHER
//
//////////////////////////////////////////////////////////////////////////////

module.exports = {
    setUp: function(cb) {
        ws = new WebSocket('ws://localhost:' + (process.env.PORT || 8080) + '/');
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
        var file = '/vagrant/examples/data/read.xml';

        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'put',
                    'path': file,
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
                    'path': file,
                },
                res: {
                    'command':      'put',
                    'status':       ghSuccess,
                    'pipelineId':   dontCare,
                }
            }]
        );
    },

    // NUMPOINTS - test command with missing 'pipeline' parameter
    // Expect: failure status
    testNumPointsMissingPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command': 'numPoints',
                },
                res: {
                    'command':  'numPoints',
                    'status':   ghFail,
                },
            }]
        );
    },

    // NUMPOINTS - test command with invalid 'pipeline' parameter
    // Expect: failure status
    testNumPointsInvalidPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'numPoints',
                    'pipeline': 'I am an invalid pipeline ID!',
                },
                res: {
                    'command':  'numPoints',
                    'status':   ghFail,
                },
            }]
        );
    },

    // NUMPOINTS - test command with object 'pipeline' parameter
    // Expect: failure status
    testNumPointsObjectPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'numPoints',
                    'pipeline':  { pipeline : 'I am an invalid pipeline ID!' }
                },
                res: {
                    'command':  'numPoints',
                    'status':   ghFail,
                },
            }]
        );
    },

    // NUMPOINTS - test command with a function as the 'pipeline' parameter
    // Expect: failure status
    testNumPointsFunctionPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'numPoints',
                    'pipeline':  function() { console.log('Wrong'); },
                },
                res: {
                    'command':  'numPoints',
                    'status':   ghFail,
                },
            }]
        );
    },

    // NUMPOINTS - test valid command
    // Expect: successful status and number of points
    testNumPointsValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'numPoints',
                    'pipeline': samplePipelineId,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'numPoints',
                    'numPoints':    10653,
                },
            }]
        );
    },

    // SCHEMA - test command with missing 'pipeline' parameter
    // Expect: failure status
    testSchemaMissingPipeline: function(test) {
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

    // SCHEMA - test command with invalid 'pipeline' parameter
    // Expect: failure status
    testSchemaInvalidPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'schema',
                    'pipeline':  'I am an invalid pipeline ID!',
                },
                res: {
                    'command':  'schema',
                    'status':   ghFail,
                },
            }]
        );
    },

    // SCHEMA - test valid command
    // Expect: Successful status and dimensions list
    testSchemaValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'schema',
                    'pipeline':  samplePipelineId,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'schema',
                    'schema':   rxSchema,
                },
            }]
        );
    },


    // STATS - test valid command
    // Expect: Successful status and dimensions list
    testStatsValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'stats',
                    'pipeline':  samplePipelineId,
                },
                res: {
                    'status':   ghSuccess,
                    'command':  'stats',
                    'stats':   dontCare,
                },
            }]
        );
    },

    // SRS - test command with missing 'pipeline' parameter
    testSrsMissingPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'srs',
                },
                res: {
                    'command':  'srs',
                    'status':   ghFail,
                },
            }]
        );
    },

    // SRS - test command with invalid 'pipeline' parameter
    testSrsInvalidPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'srs',
                    'pipeline': 'I am an invalid pipeline ID!',
                },
                res: {
                    'command':  'srs',
                    'status':   ghFail,
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
                    'command':  'srs',
                    'pipeline': samplePipelineId,
                },
                res: {
                    'command':  'srs',
                    'status':   ghSuccess,
                    'srs':      sampleSrs,
                },
            }]
        );
    },

    // FILLS - test valid command
    testFillsValid: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'fills',
                    'pipeline': samplePipelineId,
                },
                res: {
                    'command':  'fills',
                    'status':   ghSuccess,
                    'fills':    [1,4,16,64,256,1004,3330,4375,1436,160,7]
                },
            }]
        );
    },

    // READ - test command with missing 'pipeline' parameter
    // Expect: failure status
    testReadMissingPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'schema':   rendererSchema,
                },
                res: {
                    'command':  'read',
                    'status':   ghFail,
                },
            }]
        );
    },

    // READ - test command with invalid 'pipeline' parameter
    // Expect: failure status
    testReadInvalidPipeline: function(test) {
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'pipeline': 'I am an invalid pipeline ID!',
                    'schema':   rendererSchema,
                },
                res: {
                    'command':  'read',
                    'status':   ghFail,
                },
            }]
        );
    },

    // READ - test with summary flag and compression.
    // Expect: all points read followed by summary
    testReadSummary: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    0,
                    'compress': true,
                    'summary':  true,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    },
                    {
                        'command':  'summary',
                        'status':   ghSuccess,
                        'readId':   dontCare,
                        // TODO Should just check that numBytes < sampleBytes.
                        'numBytes': 104730,
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    'Wrong type!',
                },
                res:
                {
                    'status':       ghFail,
                    'command':      'read',
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    -2,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    samplePoints + 50,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            }]
        );
    },

    // READ - test request of offset >= numPoints
    // Expect: failure status, no points read
    testReadTooLargeOffset: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
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
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    10,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    10,
                        'numBytes':     10 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 10 * sampleStride;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    10,
                    'start':    -1,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    10,
                        'numBytes':     10 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 10 * sampleStride;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    20,
                    'start':    30,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    20,
                        'numBytes':     20 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 20 * sampleStride;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    20,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    20,
                        'numBytes':     20 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 20 * sampleStride;
                    }
                ]
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
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'start':    suppliedOffset,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints - suppliedOffset,
                        'numBytes':     expectedBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === expectedBytes;
                    }
                ]
            }]
        );
    },

    // READ - test that multiple reads may be issued on the same pipeline
    // Expect: both reads complete successfully
    testDoubleRead: function(test) {
        var bytesRead = 0;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
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
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    samplePoints,
                    'start':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
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
                    'command':  'cancel',
                    'pipeline': samplePipelineId,
                    'readId':   'not a valid readId'
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
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'count':    0,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    samplePoints,
                        'numBytes':     sampleBytes,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === sampleBytes;
                    }
                ]
            }]
        );
    },

    // TODO Test READ with point/radius query
    // TODO Test READ with quadtree indexed queries
    // TODO Test READ without schema parameter
    // TODO Test READ with raster queries
    // TODO Test READ with generic raster query
    // TODO Sessionless cancel

    // CANCEL - test cancel functionality and subsequent read
    // Expect: Partially transmitted data, successful cancel, successful read
    /*
    testValidCancel: function(test) {
        var bytesRead = 0;
        var bytesExpected = 214737 * sampleStride;
        doExchangeSet(
            test,
            [{
                req: {
                    'command':  'read',
                    'pipeline': bigPipelineId,
                    'schema':   rendererSchema,
                    'depthBegin':   0,
                    'depthEnd':     10,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    dontCare,
                        'numBytes':     dontCare,
                    },
                    function(data, prevResponses, json) {
                        console.log('Got res');
                        if (json) {
                            console.log(JSON.stringify(json));
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
                            console.log('Sending cancel');
                            // On the first binary blob, send the cancel.
                            if (bytesRead === 0) {
                                send({
                                    'command':  'cancel',
                                    'readId':   prevResponses[1].readId,
                                    'pipeline': bigPipelineId,
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
                    'pipeline': samplePipelineId,
                    'schema':   rendererSchema,
                    'depthBegin':   1,
                    'depthEnd':     2,
                },
                res: [
                    {
                        'status':       ghSuccess,
                        'command':      'read',
                        'readId':       dontCare,
                        'numPoints':    4,
                        'numBytes':     4 * sampleStride,
                    },
                    function(data) {
                        bytesRead += data.length;
                        return bytesRead === 4 * sampleStride;
                    }
                ]
            }]
        );
    },
    */

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

