// Unit test suite to exercise Greyhound from a websocket client.

var WebSocket = require('ws');
var timeoutMs = 3000;
var ws;

var send = function(obj) {
    ws.send(JSON.stringify(obj));
}

var setReq = function(obj) {
    ws.on('open', function() {
        send(obj);
    });
}

var setResSimple = function(test, timeoutObj, handler) {
    ws.on('message', function(data, flags) {
        handler(data, flags);
        clearTimeout(timeoutObj);
        test.done();
    });
}

var timeoutFail = function(test) {
    test.ok(false, "Request timed out!");
    test.done();
}

var dontCare = function() {
    return true;
}

var success = function(rxStatus) {
    return rxStatus === 1;
}

var fail = function(rxStatus) {
    return !success(rxStatus);
}

var validateJson = function(test, json, expected) {
    for (var field in expected) {
        test.ok(
            json.hasOwnProperty(field),
            'Missing property ' + field);

        if (typeof expected[field] !== "function") {
            test.ok(
                json[field] === expected[field],
                'Expected json[' + field + '] === ' + expected[field] +
                        ', got: ' + json[field]);
        }
        else
        {
            test.ok(
                expected[field](json[field]),
                'Validation function failed for "' + field +
                '", parameter was: ' + json[field]);
        }
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
    testPutEmptyPipeline: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'put',
            pipeline: '',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   fail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // PUT - test with malformed pipeline XML
    testPutMalformedPipeline: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'put',
            pipeline: 'I am not valid pipeline XML!',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   fail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // PUT - test with missing pipeline parameter
    testPutMissingPipelineParam: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'put',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   fail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // PUT - test double call with the same pipeline
    testPutDoublePipeline: function(test) {
        // TODO
        test.done();
    },
    
    // CREATE - test without a pipelineId parameter
    testCreateNoPipelineId: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'create',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   fail,
                    'command':  'create',
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // CREATE - test with an invalid pipeline ID
    testCreateInvalidPipelineId: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'create',
            pipelineId: 'This is not a valid pipelineId',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   fail,
                    'command':  'create',
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // CREATE - test valid command
    testCreateValid: function(test) {
        var timeoutObj = setTimeout(function() { test.done() }, timeoutMs);

        setReq({
            command: 'create',
            pipelineId: 'd4f4cc08e63242a201de6132e5f54b08',
        });

        setResSimple(test, timeoutObj, function(data, flags) {
            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   success,
                    'command':  'create',
                    'session':  dontCare,
                };

                validateJson(test, json, expected);
            } else {
                test.ok(false, 'Got unexpected binary response');
            }
        });
    },

    // CREATE - test multiple sessions created with the same pipeline
    testCreateDouble: function(test) {
        // TODO
        test.done();
    },

    // POINTSCOUNT - test command with missing 'session' parameter
    testPointsCountMissingSession: function(test) {
        // TODO
        test.done();
    },

    // POINTSCOUNT - test command with invalid 'session' parameter
    testPointsCountInvalidSession: function(test) {
        // TODO
        test.done();
    },

    // POINTSCOUNT - test valid command
    testPointsCountValid: function(test) {
        // TODO
        test.done();
    },

    // SCHEMA - test command with missing 'session' parameter
    testSchemaMissingSession: function(test) {
        // TODO
        test.done();
    },

    // SCHEMA - test command with invalid 'session' parameter
    testSchemaInvalidSession: function(test) {
        // TODO
        test.done();
    },

    // SCHEMA - test valid command
    testSchemaValid: function(test) {
        // TODO
        test.done();
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
    testReadMissingSession: function(test) {
        // TODO
        test.done();
    },

    // READ - test command with invalid 'session' parameter
    testReadInvalidSession: function(test) {
        // TODO
        test.done();
    },

    // READ - test request of zero points
    testReadZeroPoints: function(test) {
        // TODO
        test.done();
    },

    // READ - test negative number of points requested
    testReadNegativeNumPoints: function(test) {
        // TODO
        test.done();
    },

    // READ - test request of more points than exist in the pipeline
    testReadTooManyPoints: function(test) {
        // TODO
        test.done();
    },

    // READ - test request of offset > numPoints
    testReadTooLargeOffset: function(test) {
        // TODO
        test.done();
    },

    // READ - test negative offset requested
    testReadNegativeOffset: function(test) {
        // TODO
        test.done();
    },

    // READ - test get complete buffer
    testReadAll: function(test) {
        // TODO
        test.done();
    },

    // READ - test with non-zero count and offset
    testReadCountAndOffset: function(test) {
        // TODO
        test.done();
    },

    // READ - test missing offset
    testReadNoOffsetSupplied: function(test) {
        // TODO
        test.done();
    },

    // READ - test missing count
    testReadNoCountSupplied: function(test) {
        // TODO
        test.done();
    },

    // DESTROY - test command with missing 'session' parameter
    testDestroyMissingSession: function(test) {
        // TODO
        test.done();
    },

    // DESTROY - test command with invalid 'session' parameter
    testDestroyInvalidSession: function(test) {
        // TODO
        test.done();
    },

    // DESTROY - test valid destroy
    testDestroyValid: function(test) {
        // TODO
        test.done();
    },

    // OTHER - test non-existent command
    testOtherBadCommand: function(test) {
        // TODO
        test.done();
    },

    // OTHER - test missing 'command' parameter
    testOtherMissingCommand: function(test) {
        // TODO
        test.done();
    },

    // OTHER - test empty 'command' parameter
    testOtherEmptyCommand: function(test) {
        // TODO
        test.done();
    },
};

