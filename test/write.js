var common = require('./common');
var server = common.server;
var resource = common.resource;
var util = require('./util');

var chai = require('chai');
var chaiHttp = require('chai-http');
var should = chai.should();
var expect = chai.expect;
chai.use(chaiHttp);

var Promise = require('bluebird');
var info = util.httpSync('/info');

var writeSchema = [
    { name: 'X-Gt-0', type: 'unsigned', size: 1 }
];

describe('write', () => {
    it('404s nonexistent resources', (done) => {
        new Promise((resolve, reject) => {
            chai.request(server).put('/resource/i-do-not-exist/write')
            .end((err, res) => resolve(res));
        })
        .then((res) => {
            res.should.have.status(404);
            done();
        })
        .catch((err) => done(err));
    });

    it('400s unnamed append-schemas', (done) => {
        util.write({ schema: [{ name: 'A', type: 'signed', size: 1 }] })
        .then((res) => {
            res.should.have.status(400);
            done();
        })
        .catch((err) => done(err));
    });

    var registerTestAppend = () => {
        return util.write({
            name: 'testing',
            schema: [{ name: 'A', type: 'signed', size: 1 }]
        })
        .then((res) => {
            res.should.have.status(200);
        })
        .catch((err) => done(err));
    };

    it('400s registration of the same dimension with a new type', (done) => {
        registerTestAppend()
        .then(() => {
            return util.write({
                name: 'testing',
                schema: [{ name: 'A', type: 'unsigned', size: 1 }]
            })
            .then((res) => {
                res.should.have.status(400);
                done();
            });
        })
        .catch((err) => done(err));
    });

    it('400s registration of the same dimension with a new size', (done) => {
        registerTestAppend()
        .then(() => {
            return util.write({
                name: 'testing',
                schema: [{ name: 'A', type: 'signed', size: 2 }]
            })
            .then((res) => {
                res.should.have.status(400);
                done();
            });
        })
        .catch((err) => done(err));
    });

    it('400s registration of a different schema with the same name', (done) => {
        registerTestAppend()
        .then(() => {
            // Can't register a different schema under the same set name.
            return util.write({
                name: 'testing',
                schema: [{ name: 'B', type: 'unsigned', size: 1 }]
            })
            .then((res) => {
                res.should.have.status(400);
                done();
            });
        })
        .catch((err) => done(err));
    });

    it('400s registration of the same dimension with a new set', (done) => {
        registerTestAppend()
        .then(() => {
            // Can't register the exact same dimension with a new set.
            return util.write({
                name: 'testing2',
                schema: [{ name: 'A', type: 'signed', size: 1 }]
            })
            .then((res) => {
                res.should.have.status(400);
                done();
            });
        })
        .catch((err) => done(err));
    });

    it('400s registration of the same dim-name with a new set', (done) => {
        registerTestAppend()
        .then(() => {
            // Can't register a dimension with the same name with a new set.
            return util.write({
                name: 'testing2',
                schema: [{ name: 'A', type: 'floating', size: 4 }]
            })
            .then((res) => {
                res.should.have.status(400);
                done();
            })
        })
        .catch((err) => done(err));
    });

    it('Appends dimensions to all points', (done) => {
        util.read({ schema: util.xyz })
        .then((res) => {
            res.should.have.status(200);

            var numPoints = util.numPointsFrom(res.body, util.xyz);
            var readPointSize = util.pointSizeFrom(util.xyz);
            var readBuffer = res.body;
            var readNumBytes = numPoints * readPointSize;
            expect(numPoints).to.equal(info.numPoints);

            var writePointSize = util.pointSizeFrom(writeSchema);
            var writeBuffer = new ArrayBuffer(writePointSize * numPoints);

            var p = { x: 0, y: 0, z: 0 };

            var processPoint = (p, v) => {
                v.setUint8(0, p.x > 0 ? 1 : 0);
            };

            for (var i = 0; i < numPoints; ++i) {
                var readView = new DataView(readBuffer, readPointSize * i);
                p = {
                    x: readView.getFloat32(0),
                    y: readView.getFloat32(8),
                    z: readView.getFloat32(12)
                };

                var writeView = new DataView(writeBuffer, writePointSize * i);
                processPoint(p, writeView);
            }

            return util.write({
                name: 'testing-dims',
                schema: writeSchema
            }, writeBuffer);
        })
        .then((res) => {
            res.should.have.status(200);

            done();
        })
        .catch((err) => done(err));
    });
});

