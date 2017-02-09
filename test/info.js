var common = require('./common');
var server = common.server;
var resource = common.resource;

var chai = require('chai');
var chaiHttp = require('chai-http');
var should = chai.should();
var expect = chai.expect;
chai.use(chaiHttp);

describe('info', () => {
    it('404s nonexistent resources', (done) => {
        chai.request(server).get('/resource/asdf/info')
        .end((err, res) => {
            res.should.have.status(404);
            done();
        });
    });

    it('returns JSON metadata', (done) => {
        chai.request(server).get(resource + '/info')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('object');

            var r = res.body;

            r.should.have.property('baseDepth');
            r.baseDepth.should.be.a('number');

            r.should.have.property('bounds');
            r.bounds.should.be.an('array');
            r.bounds.should.have.lengthOf(6);

            r.should.have.property('boundsConforming');
            r.boundsConforming.should.be.an('array');
            r.boundsConforming.should.have.lengthOf(6);

            r.should.have.property('numPoints');
            r.numPoints.should.equal(100000);

            r.should.have.property('offset');
            r.offset.should.be.an('array');
            r.offset.should.have.lengthOf(3);

            r.should.have.property('scale');
            r.scale.should.be.a('number');
            r.scale.should.equal(0.01);

            r.should.have.property('schema');
            r.schema.should.be.an('array');
            r.schema.forEach((k, v) => {
                k.should.be.an('object');

                k.should.have.property('name');
                k.name.should.be.a('string');

                k.should.have.property('type');
                k.type.should.be.a('string');
                k.type.should.be.oneOf(['signed', 'unsigned', 'floating']);

                k.should.have.property('size');
                k.size.should.be.a('number');
                k.size.should.be.oneOf([1, 2, 4, 8]);
            });

            r.should.have.property('srs');
            r.srs.should.be.a('string');

            r.should.have.property('type');
            r.type.should.equal('octree');

            done();
        });
    });
});

