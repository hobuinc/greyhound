var path = require('path');
var fs = require('fs');
var aws;

var home =
    process.env.HOME ||
    process.env.USERPROFILE ||
    (process.env.HOMEDRIVE + process.env.HOMEPATH);

var user = process.env.AWS_PROFILE || 'default';

if (home) {
    var credsPath = path.join(home, '.aws/credentials');

    if (fs.existsSync(credsPath)) {
        var opt = { encoding: 'utf8' };
        var creds = fs.readFileSync(credsPath, opt).split('\n');

        var i = 0;

        var accessRegex = /^aws_access_key_id\s*=\s*(\S+)\s*$/;
        var hiddenRegex = /^aws_secret_access_key\s*=\s*(\S+)\s*$/;

        while (i < creds.length - 2 && !aws) {
            if (creds[i] == '[' + user + ']') {
                var accessMatch = creds[i + 1].match(accessRegex);
                var hiddenMatch = creds[i + 2].match(hiddenRegex);

                if (accessMatch && hiddenMatch) {
                    aws = {
                        'access': accessMatch[1],
                        'hidden': hiddenMatch[1]
                    };
                }
            }

            ++i;
        }
    }
}

module.exports = aws;

