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
var request = require('sync-request');

var info = util.httpSync('/info');
expect(info.numPoints).to.equal(100000);
var bounds = {
    xmin: info.bounds[0], ymin: info.bounds[1], zmin: info.bounds[2],
    xmax: info.bounds[3], ymax: info.bounds[4], zmax: info.bounds[5]
};

var files = util.httpSync('/files?bounds=[-5,-5,-5,5,5,5]');

var schema = info.schema;
schema[0].type = 'floating'; schema[0].size = 4;
schema[1].type = 'floating'; schema[1].size = 4;
schema[2].type = 'floating'; schema[2].size = 4;

var getOffset = (name, schema) => {
    var offset = 0;
    for (var i = 0; i < schema.length; ++i) {
        if (schema[i].name == name) return offset;
        else offset += schema[i].size;
    }
    throw new Error('No offset found for ' + name);
};

var buffer = request(
        'GET', server + resource + '/read?schema=' + JSON.stringify(schema))
        .getBody().buffer;

var makeKey = (x, y, z) =>
    x.toString() + '-' + y.toString() + '-' + z.toString();

var pointMap = (() => {
    var numPoints = util.numPointsFrom(buffer, schema);
    var pointSize = util.pointSizeFrom(schema);

    var map = { };
    var x, y, z;

    var view = new DataView(buffer);
    for (var offset = 0; offset < numPoints * pointSize; offset += pointSize) {
        x = view.getFloat32(offset, true);
        y = view.getFloat32(offset + 4, true);
        z = view.getFloat32(offset + 8, true);
        expect(x).to.be.within(bounds.xmin, bounds.xmax);
        expect(y).to.be.within(bounds.ymin, bounds.ymax);
        expect(z).to.be.within(bounds.zmin, bounds.zmax);

        map[makeKey(x, y, z)] = offset;
    }
    return map;
})();

describe('filter', () => {
    it('rejects invalid dimensions', (done) => {
        util.read({ filter: { 'Asdf': 42 } })
        .then((res) => {
            res.should.have.status(400);
            done();
        });
    });

    it('rejects invalid filter types', (done) => {
        var checks = [];
        checks.push(util.read({ filter: 42 })
        .then((res) => res.should.have.status(400)));

        checks.push(util.read({ filter: 'asdf' })
        .then((res) => res.should.have.status(400)));

        Promise.all(checks).then(() => done());
    });

    var checkOrigin = (index, filter) => {
        var fileInfo = files[index];
        var origin = fileInfo.origin;
        var path = fileInfo.path;

        return util.read({ schema: schema, filter: filter })
        .then((res) => {
            res.should.have.status(200);
            var view = new DataView(res.body);
            var numPoints = util.numPointsFrom(res.body, schema);
            var pointSize = util.pointSizeFrom(schema);
            var numBytes = numPoints * pointSize;
            var originOffset = getOffset('OriginId', schema);

            expect(numPoints).to.equal(fileInfo.numPoints);

            for (var offset = 0; offset < numBytes; offset += pointSize) {
                var v = view.getUint32(offset + originOffset, true);
                expect(v).to.equal(origin);
            }

            return numPoints;
        });
    };

    it('filters by path', (done) => {
        var checks = files.reduce((p, c, i) => {
            return p.concat(checkOrigin(i, { Path: c.path }));
        }, []);

        Promise.all(checks).then((results) => {
            var numPoints = results.reduce((p, c) => p + c, 0);
            expect(numPoints).to.equal(info.numPoints);
            done();
        });
    });

    it('filters by origin', (done) => {
        var checks = files.reduce((p, c, i) => {
            return p.concat(checkOrigin(i, { OriginId: c.origin }));
        }, []);

        Promise.all(checks).then((results) => {
            var numPoints = results.reduce((p, c) => p + c, 0);
            expect(numPoints).to.equal(info.numPoints);
            done();
        });
    });

    it('filters on inequalities', (done) => {
        var checks = [
            util.read({
                schema: schema,
                filter: { Intensity: { "$lte": 128 } }
            }),
            util.read({
                schema: schema,
                filter: { Intensity: { "$gt": 128 } }
            })
        ];

        Promise.all(checks).spread((lte, gt) => {
            var pointSize = util.pointSizeFrom(schema);
            var offset = getOffset('Intensity', schema);
            var check = (buffer, validate) => {
                var numPoints = util.numPointsFrom(buffer, schema);
                var view = new DataView(buffer);
                for (var i = 0; i < numPoints; ++i) {
                    var v = view.getUint16(i * pointSize + offset, true);
                    validate(v);
                }
                return numPoints;
            };
            var l = check(lte.body, (v) => expect(v).to.be.at.most(128));
            var g = check(gt.body, (v) => {
                expect(v).to.be.above(128);
                expect(v).to.be.at.most(255);
            });
            expect(l + g).to.equal(info.numPoints);
            done();
        });
    });
});

