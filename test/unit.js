// Unit test suite to exercise Greyhound from a websocket client.

var fs = require('fs');
var WebSocket = require('ws');
var ws, timeoutObj;
var timeoutMs = 3000;
var samplePipelineId = 'd4f4cc08e63242a201de6132e5f54b08';

var send = function(obj) {
    ws.send(JSON.stringify(obj));
}

var setInitialCmd = function(obj) {
    ws.on('open', function() {
        send(obj);
    });
}

var endTest = function(test) {
    clearTimeout(timeoutObj);
    test.done();
}

var setResponseFsm = function(handler) {
    ws.on('message', handler);
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
        else {
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
    // Expect: failure status
    testPutEmptyPipeline: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'put',
            pipeline: '',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // PUT - test with malformed pipeline XML
    // Expect: failure status
    testPutMalformedPipeline: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'put',
            pipeline: 'I am not valid pipeline XML!',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // PUT - test with missing pipeline parameter
    // Expect: failure status
    testPutMissingPipelineParam: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'put',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'put',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // PUT - test double call with the same pipeline (this also tests
    // the nominal case)
    // Expect: Two successful statuses with a pipelineId parameter in each
    // response
    testPutDoublePipeline: function(test) {
        timeoutObj = startTestTimer(test);

        var got = 0;
        var filename = '/vagrant/examples/data/read.xml';

        fs.readFile(filename, 'utf8', function(err, file) {
            var command = {
                    command:    'put',
                    pipeline:   file,
            };

            if (err) {
                test.ok(false, 'Error reading pipeline: ' + filename)
                endTest(test);
            }

            setInitialCmd(command);

            setResponseFsm(function(data, flags) {
                ++got;

                test.ok(!flags.binary, 'Got unexpected binary response');

                if (!flags.binary) {
                    var json = JSON.parse(data);
                    var expected = {
                        'status':       ghSuccess,
                        'command':      'put',
                        'pipelineId':   dontCare,
                    };

                    validateJson(test, json, expected);
                }

                if (got === 1) {
                    send(command);
                }
                else if (got === 2) {
                    endTest(test);
                }
            });
        });
    },
    
    // CREATE - test without a pipelineId parameter
    // Expect: failure status
    testCreateNoPipelineId: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'create',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'create',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // CREATE - test with an invalid pipeline ID
    // Expect: failure status
    testCreateInvalidPipelineId: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'create',
            pipelineId: 'This is not a valid pipelineId',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'create',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // CREATE - test valid command
    // Expect: successful status and 'session' parameter in response
    testCreateValid: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command:    'create',
            pipelineId: samplePipelineId,
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghSuccess,
                    'command':  'create',
                    'session':  dontCare,
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
    },

    // CREATE - test multiple sessions created with the same pipeline
    testCreateDouble: function(test) {
        timeoutObj = startTestTimer(test);
        var got = 0;
        var firstSessionId = '';
        var secondSessionId = '';
        var cmd = {
            command:    'create',
            pipelineId: samplePipelineId,
        };

        setInitialCmd(cmd);

        setResponseFsm(function(data, flags) {
            ++got;

            test.ok(!flags.binary, 'Got unexpected binary response');
            if (flags.binary) endTest(test);

            var json = JSON.parse(data);

            if (got === 1) {
                var expected = {
                    'status':   ghSuccess,
                    'command':  'create',
                    'session':  dontCare,
                };

                validateJson(test, json, expected);

                firstSessionId = json['session'];

                send(cmd);
            }
            else if (got === 2) {
                var expected = {
                    'status':   ghSuccess,
                    'command':  'create',
                    'session':  function(actual) {
                        return actual !== firstSessionId
                    },
                };

                validateJson(test, json, expected);

                secondSessionId = json['session'];

                send({ command: 'destroy', 'session': firstSessionId });
            }
            else if (got === 3) {
                var expected = {
                    'status':   ghSuccess,
                    'command':  'destroy',
                };

                validateJson(test, json, expected);

                send({ command: 'destroy', 'session': secondSessionId });
            }
            else if (got === 4) {
                var expected = {
                    'status':   ghSuccess,
                    'command':  'destroy',
                };

                validateJson(test, json, expected);

                endTest(test);
            }
        });
    },

    // POINTSCOUNT - test command with missing 'session' parameter
    testPointsCountMissingSession: function(test) {
        timeoutObj = startTestTimer(test);

        setInitialCmd({
            command: 'pointsCount',
        });

        setResponseFsm(function(data, flags) {
            test.ok(!flags.binary, 'Got unexpected binary response');

            if (!flags.binary) {
                var json = JSON.parse(data);
                var expected = {
                    'status':   ghFail,
                    'command':  'pointsCount',
                };

                validateJson(test, json, expected);
            }

            endTest(test);
        });
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

