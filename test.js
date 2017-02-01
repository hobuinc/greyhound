var requests = JSON.parse(require('fs').readFileSync('requests.json'));
var _ = require('lodash');

var Controller = require('./src/controller').Controller;
var config = JSON.parse(require('fs').readFileSync('csmannin/uncommented.json'));

var controller = new Controller(config);
var bytes = 0;
var dones = 0;
var a = 0;

console.log('Requests:', requests.length);

var go = () => {
    requests.forEach((v, i) => {
        controller.read(
                v.resource,
                _.merge({ }, v.query),
                (err) => {
                    if (err) console.log('INIT ERR:', err);
                },
                (err, data, done) => {
                    if (err) console.log('DATA ERR:', err);
                    else bytes += data.length;

                    if (done) {
                        ++dones;
                        if (dones == requests.length) {
                            dones = 0;
                            console.log('all done');
                            global.gc();
                            ++a;
                        }
                    }

                    return true;
                });
    });
};

go();
setTimeout(go, 12000);

