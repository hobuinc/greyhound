var common = require('./common');
var server = common.server;
var resource = common.resource;

var chai = require('chai');
var chaiHttp = require('chai-http');
var should = chai.should();
var expect = chai.expect;
chai.use(chaiHttp);

var toArrayBuffer = (s) => {
    var b = new ArrayBuffer(s.length);
    var view = new Uint8Array(b);
    for (var i = 0; i < b.byteLength; ++i) {
        view[i] = s.charCodeAt(i);
    }
    return b;
};

var toString = (b) => String.fromCharCode.apply(null, new Uint8Array(b));

var parseBinary = (res, cb) => {
    res.setEncoding('binary');
    res.data = '';
    res.on('data', (chunk) => res.data += chunk);
    res.on('end', () => cb(null, toArrayBuffer(res.data)));
};

var pointSizeFrom = (schema) => {
    return schema.reduce((p, c) => p + c.size, 0);
};

var numPointsFrom = (buffer, schema) => {
    var view = new DataView(buffer);
    var numBytes = buffer.byteLength - 4;
    var numPoints = view.getUint32(numBytes, true);
    var pointSize = pointSizeFrom(schema);
    expect(numPoints).to.equal(numBytes / pointSize);
    return numPoints;
};

var split = (b, randomize) => {
    var factor = [0.5, 0.5, 0.5];
    if (randomize) {
        var r = () => 0.5 + (Math.random() - 0.5) / 2;
        factor = [r(), r(), r()];
    }

    var mid = [
        b[0] + (b[3] - b[0]) * factor[0],
        b[1] + (b[4] - b[1]) * factor[1],
        b[2] + (b[5] - b[2]) * factor[2]
    ];
    var splits = [];
    for (var i = 0; i < 8; ++i) {
        var c = b.slice();
        if (i < 4) c[0] = mid[0]; else c[3] = mid[0];
        if (i % 4 < 2) c[1] = mid[1]; else c[4] = mid[1];
        if (i % 2) c[2] = mid[2]; else c[5] = mid[2];
        splits.push(c);
    }
    return splits;
};

var request = require('sync-request');
var httpSync = (path) =>
    JSON.parse(request('GET', server + resource + path).getBody());

var xyz = [
    { name: 'X', type: 'floating', size: 4 },
    { name: 'Y', type: 'floating', size: 4 },
    { name: 'Z', type: 'floating', size: 4 }
];

var read = (query) => {
    if (!query) query = { };
    var path = resource + '/read' + Object.keys(query).reduce((p, c) => {
        return p + (p.length ? '&' : '?') + c + '=' + JSON.stringify(query[c]);
    }, '');

    return new Promise((resolve, reject) => {
        chai.request(server).get(path)
        .buffer()
        .parse(parseBinary)
        .end((err, res) => resolve(res));
    });
};

var write = (query, data) => {
    if (!query) query = { };
    if (!data) data = new ArrayBuffer(0);
    var path = resource + '/write' + Object.keys(query).reduce((p, c) => {
        return p + (p.length ? '&' : '?') + c + '=' + JSON.stringify(query[c]);
    }, '');

    return new Promise((resolve, reject) => {
        chai.request(server).put(path)
        .send(data)
        .end((err, res) => resolve(res));
    });
};

var getOffset = (name, schema) => {
    var offset = 0;
    for (var i = 0; i < schema.length; ++i) {
        if (schema[i].name == name) return offset;
        else offset += schema[i].size;
    }
    throw new Error('No offset found for ' + name);
};

var getSize = (name, schema) => {
    var d = schema.find((v) => v.name == name);
    if (d) return d.size;
    else throw new Error('No size found for ' + name);
}

module.exports = {
    toArrayBuffer: toArrayBuffer,
    toString: toString,
    pointSizeFrom: pointSizeFrom,
    numPointsFrom: numPointsFrom,
    split: split,
    httpSync: httpSync,
    xyz: xyz,
    read: read,
    write: write,
    getOffset: getOffset,
    getSize: getSize
};

