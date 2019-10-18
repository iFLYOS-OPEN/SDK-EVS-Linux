var EasyCurl = require('curl').EasyCurl;
var fs = require('fs');
var console = require('console');

function testBuffer() {
    var curl = new EasyCurl();
    curl = new EasyCurl();//test destructor
    curl.setOpt(EasyCurl.CURLOPT_URL, 'http://cdn.iflyos.cn/device-wifi/' + EasyCurl.escape('wifistub.html'));
    curl.setOpt(EasyCurl.CURLOPT_FOLLOWLOCATION, 1);
    curl.perform((err, data, finish) => {
        if (err) {
            console.log('curl error:', err);
            return;//throw err;
        }
        console.log('write bytes ', data.length, !!finish);
        console.log('content:', data);
        if (finish) {
            console.log('buffer download finish');
        }
    });
}

function testStream() {
    var wr = fs.createWriteStream('/tmp/downloadtest');
    wr.on('open', () => {
        var curl = new EasyCurl({stream: true});
        curl.setOpt(EasyCurl.CURLOPT_URL, 'https://github.com/Kitt-AI/snowboy/blob/master/resources/models/computer.umdl?' + EasyCurl.escape('raw=true'));
        curl.setOpt(EasyCurl.CURLOPT_FOLLOWLOCATION, 1);
        curl.perform((err, data, finish) => {
            if (err) {
                console.log('curl error:', err);
                throw err;
            }
            console.log('write bytes ', data.length, !!finish);
            if(data.length > 0) {
                wr.write(data);
            }
            if (finish) {
                wr.end();
                console.log('stream download finish');
            }
        });
    });
}

testBuffer();
//testStream();
_gc();

console.log('run here');
setInterval(()=>{}, 100000);