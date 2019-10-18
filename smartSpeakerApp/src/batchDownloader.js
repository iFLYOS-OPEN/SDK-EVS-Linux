var EventEmitter = require('events');
var Log = require('console').Log;
var EasyCurl = require('curl');
var fs = require('fs');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('downloader'));

class BatchDownloader extends EventEmitter{
    /**
     *
     * @param {Array}urls urls to download
     * @param target target dir
     */
    constructor(tasks, dir){
        super();
        this.tasks = tasks;
        if(dir[dir.length - 1] != '/'){
            dir = dir + '/';
        }
        this.dir = dir;
        this.curl = undefined;
        try {
            fs.mkdirSync(this.dir);
        }catch (e) {
            //already exists
        }
    }

    start(){
        var promise = Promise.resolve();
        this.tasks.forEach((task)=>{
            promise = promise.then(()=>this._download(task));
        });

        promise
            .then(()=>this.emit('done'))
            .catch(err=> {
                this.emit('error', err);
            });
    }

    _download(task) {
        return new Promise((ok, fail) => {
            var destPath = this.dir + task.name;
            try {
                fs.unlinkSync(destPath);
            }catch (e) {
                //file not exists
            }
            var wr = fs.createWriteStream(destPath);
            this.wr = wr;
            wr.on('open', () => {
                var req = this.curl = new EasyCurl({stream: true});
                req.setOpt(EasyCurl.CURLOPT_TIMEOUT, 120);
                req.setOpt(EasyCurl.CURLOPT_URL, task.url);
                req.perform((err, data, end) => {
                    if (err) {
                        log('download', task.url, 'error', err);
                        fail(err);
                        wr.end();
                        return;
                    }

                    if (end) {
                        log('download', task.url, 'to', destPath, 'finished');
                        wr.end();
                        ok(destPath);
                        return;
                    }
                    wr.write(data);
                });
            });
        });
    }

    cancel(){
        if(this.wr){
            this.wr.end();
            this.wr = undefined;
        }
        if(this.curl){
            this.curl.cancel();
            this.curl = undefined;
        }
    }

    static _uuid(){
        var buf = fs.readFileSync('/proc/sys/kernel/random/uuid');
        return buf.toString(-1, 0, buf.length - 1);
    }
}

module.exports.BatchDownler = BatchDownloader;