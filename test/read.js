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
var bounds = {
    xmin: info.bounds[0], ymin: info.bounds[1], zmin: info.bounds[2],
    xmax: info.bounds[3], ymax: info.bounds[4], zmax: info.bounds[5]
};

describe('read', () => {
    it('404s nonexistent resources', (done) => {
        chai.request(server).get('/resource/asdf/read')
        .end((err, res) => {
            res.should.have.status(404);
            done();
        });
    });

    it('reads all existing data', (done) => {
        var schema = util.xyz;
        util.read({ schema: schema })
        .then((res) => {
            res.should.have.status(200);

            var numPoints = util.numPointsFrom(res.body, schema);
            var pointSize = util.pointSizeFrom(schema);
            var numBytes = numPoints * pointSize;
            expect(numPoints).to.equal(info.numPoints);

            var view = new DataView(res.body);
            var x, y, z;

            for (var offset = 0; offset < numBytes; offset += pointSize)
            {
                x = view.getFloat32(offset, true);
                y = view.getFloat32(offset + 4, true);
                z = view.getFloat32(offset + 8, true);
                expect(x).to.be.within(bounds.xmin, bounds.xmax);
                expect(y).to.be.within(bounds.ymin, bounds.ymax);
                expect(z).to.be.within(bounds.zmin, bounds.zmax);
            }

            done();
        });
    });

    it('works with depth ranges', (done) => {
        var schema = util.xyz;
        var depths = [];
        var depthEnd = 16;
        for (var i = 0; i < depthEnd; ++i) depths.push(i);

        var reads = depths.reduce((p, c, i, a) => {
            return p.concat(util.read({
                schema: schema,
                depthBegin: a[i],
                depthEnd: i == depthEnd - 1 ? 128 : a[i] + 1
            }));
        }, []);

        // TODO This depth is kind of magical at the moment, should probably
        // expose it publicly through `info`.
        var startDepth = 6;
        var hier = util.httpSync('/hierarchy' +
                '?depthBegin=' + startDepth +
                '&depthEnd=' + depthEnd + '&vertical=true');

        Promise.all(reads).then((results) => {
            var numPoints = results.reduce((p, c) => {
                return p + util.numPointsFrom(c.body, schema);
            }, 0);

            for (var i = startDepth; i < results.length; ++i) {
                if (i - startDepth < hier.length) {
                    expect(hier[i - startDepth]).to.equal(
                            util.numPointsFrom(results[i].body, schema));
                }
            }

            expect(numPoints).to.equal(info.numPoints);
            done();
        });
    });

    var climb = (randomize) => {
        var concurrent = 12;
        var schema = util.xyz;
        var pointSize = util.pointSizeFrom(schema);
        var bounds = info.bounds;
        var baseDepth = 7;

        // If we get back a small response, don't subdivide.
        var stopSplitNumPoints = 2048;

        var numPoints = 0;
        var outstanding = 0;

        var traverse = (bounds, depth) => {
            var p = new Promise((resolve) => {
                if (outstanding < concurrent) {
                    ++outstanding;

                    var hier;
                    if (!randomize) {
                        hier = util.httpSync(
                                '/hierarchy?depth=' + depth +
                                '&bounds=' + JSON.stringify(bounds));
                    }

                    util.read({ schema: schema, bounds: bounds, depth: depth })
                    .then((result) => {
                        --outstanding;

                        var n = util.numPointsFrom(result.body, schema);
                        numPoints += n;
                        var next = [];

                        if (!randomize) expect(n).to.equal(hier ? hier.n : 0);

                        if (n) {
                            if (n < stopSplitNumPoints) {
                                next.push(traverse(bounds, depth + 1));
                            }
                            else util.split(bounds, randomize).forEach((b) => {
                                next.push(traverse(b, depth + 1));
                            });

                            return Promise.all(next);
                        }
                        else return null;
                    })
                    .then(resolve);
                }
                else setTimeout(() => {
                    traverse(bounds, depth).then(resolve);
                }, 200);
            });

            return p;
        };

        return util.read({ schema: schema, depthEnd: baseDepth })
        .then((result) => numPoints += util.numPointsFrom(result.body, schema))
        .then(() => traverse(bounds, baseDepth))
        .then(() => numPoints);
    };

    it('works with traversal-split bounds', (done) => {
        climb()
        .then((numPoints) => expect(numPoints).to.equal(info.numPoints))
        .then(() => done());
    });

    it('works with arbitrarily split bounds', (done) => {
        climb(true)
        .then((numPoints) => expect(numPoints).to.equal(info.numPoints))
        .then(() => done());
    });
});

