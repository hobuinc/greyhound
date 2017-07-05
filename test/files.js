var common = require('./common');
var server = common.server;
var resource = common.resource;

var chai = require('chai');
var chaiHttp = require('chai-http');
var should = chai.should();
var expect = chai.expect;
chai.use(chaiHttp);

describe('files', () => {
    it('404s nonexistent resources', (done) => {
        chai.request(server).get('/resource/asdf/files/0')
        .end((err, res) => {
            res.should.have.status(404);
            done();
        });
    });

    it('Returns the file count on empty searches', (done) => {
        chai.request(server).get(resource + '/files')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.a('number');
            res.body.should.equal(8);
            done();
        });
    });

    it('400s searches with both bounds and files', (done) => {
        var q = '/files?bounds=[0,0,0,1,1,1]&search=0'
        chai.request(server).get(resource + q)
        .end((err, res) => {
            res.should.have.status(400);
            done();
        });
    });

    var check = (r, origin, path) => {
        r.should.have.property('bounds');
        r.bounds.should.be.an('array');
        r.bounds.should.have.lengthOf(6);

        r.should.have.property('metadata');
        r.metadata.should.be.an('object');

        r.should.have.property('numPoints');
        r.numPoints.should.be.a('number');

        r.should.have.property('origin');
        r.origin.should.be.a('number');
        if (origin != null) r.origin.should.equal(origin);

        r.should.have.property('path');
        r.path.should.be.a('string');
        if (path != null) r.path.should.match(new RegExp(path));

        r.should.have.property('pointStats');
        r.pointStats.should.be.an('object');
        r.pointStats.should.have.property('inserts');
        r.pointStats.should.have.property('outOfBounds');
        r.pointStats.should.have.property('overflows');

        r.should.have.property('status');
        r.status.should.equal('inserted');
    };

    it('returns file info by origin ID', (done) => {
        chai.request(server).get(resource + '/files/0')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('object');
            check(res.body, 0);
            done();
        });
    });

    it('returns file info by query-parameters origin ID', (done) => {
        chai.request(server).get(resource + '/files?search=0')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('object');
            check(res.body, 0);
            done();
        });
    });

    it('returns file info by filename match', (done) => {
        chai.request(server).get(resource + '/files/nwd')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('object');
            check(res.body, null, 'nwd');
            done();
        });
    });

    it('returns file info by query-parameter filename match', (done) => {
        chai.request(server).get(resource + '/files?search="nwd"')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('object');
            check(res.body, null, 'nwd');
            done();
        });
    });

    it('returns multiple file-info entries array of origins', (done) => {
        chai.request(server).get(resource + '/files?search=[0,1,2,3,4,5,6,7]')
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('array');
            res.body.should.have.lengthOf(8);
            res.body.forEach((v, i) => check(v, i));
            done();
        });
    });

    it('returns multiple file-info entries from array of matches', (done) => {
        var order = ['nwd', 'nwu', 'ned', 'neu', 'swd', 'swu', 'sed', 'seu'];
        chai.request(server).get(
                resource + '/files?search=' + JSON.stringify(order))
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('array');
            res.body.should.have.lengthOf(8);
            res.body.forEach((v, i) => check(v, null, order[i]));
            done();
        });
    });

    it('returns multiple file-info entries from mixed searches', (done) => {
        var search = ['nwd', 0, 4, 'swu'];
        chai.request(server).get(
                resource + '/files?search=' + JSON.stringify(search))
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('array');
            res.body.should.have.lengthOf(4);
            res.body.forEach((v, i) => {
                if (typeof(search[i]) == 'number') check(v, search[i]);
                else check(v, null, search[i]);
            });
            done();
        });
    });

    it('returns null for not-found origin IDs', (done) => {
        chai.request(server).get(resource + '/files/8')
        .end((err, res) => {
            res.should.have.status(200);
            expect(res.body).to.be.null;
            done();
        });
    });

    it('returns null for not-found paths', (done) => {
        chai.request(server).get(resource + '/files/asdf')
        .end((err, res) => {
            res.should.have.status(200);
            expect(res.body).to.be.null;
            done();
        });
    });

    it('inserts nulls for not-found search array entries', (done) => {
        var search = ['nwd', 'asdf', 0, 4, 8, 'swu'];

        chai.request(server).get(
                resource + '/files?search=' + JSON.stringify(search))
        .end((err, res) => {
            res.should.have.status(200);
            res.body.should.be.an('array');
            res.body.should.have.lengthOf(6);
            res.body.forEach((v, i) => {
                if (typeof(search[i]) == 'number') {
                    if (search[i] >= 0 && search[i] < 8) {
                        check(v, search[i]);
                    }
                    else expect(v).to.be.null;
                }
                else {
                    if (search[i] != 'asdf') check(v, null, search[i]);
                    else expect(v).to.be.null;
                }
            });
            done();
        });
    });
});

