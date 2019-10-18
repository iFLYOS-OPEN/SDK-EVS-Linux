var http = require('http');
var EventEmitter = require('events');
var Log = require('console').Log;

class InternetMonitor extends EventEmitter{
    constructor(params){
        super();
        var logger = new Log('InternetMonitor');
        this.log = logger.log.bind(logger);
        this.timeout= 3000;
        this.interval = (params && params.interval) || 30000;//30秒检查一次网络
        this.ok = false;
        this.timeoutTimer = -1;
        this.quit = false;
        this.gotTimestamp = false;
        this.httpOptions = {
            host: 'easybox.iflyos.cn',
            path: '/wifistub',
            method: 'GET',
        };
        this.timeStampHttpOptions = {
            host: 'easybox.iflyos.cn',
            path: '/time_now',
            method: 'GET',
        };
        this.failTime = 0;
    }

    _nextCheck(){
        clearTimeout(this.timeoutTimer);
        if(this.ok) {
            setTimeout(this._check.bind(this), this.interval);
        }else{
            setTimeout(this._check.bind(this), 2000);//2s
        }
    }

    _onSuccess(){
        this.log('success');
        this.failTime = 0;
        if(!this.ok){
            this.ok = true;
            this.emit('ok');
        }
        this._nextCheck();
    }

    _onFail(){
        this.log('fail');
        if(++this.failTime < 3){
            clearTimeout(this.timeoutTimer);
            this._check();
            return;
        }
        if(this.ok){
            this.ok = false;
            this.emit('fail');
        }
        this._nextCheck();
    }

    _check(){
        if(this.quit) return;
        var opt = this.gotTimestamp ? this.httpOptions : this.timeStampHttpOptions;
        this.log("perform", opt.method, opt.host, opt.path);
        var req = http.request(opt);
        //req.setTimeout(this.timeout);
        req.on('response', (resp)=>{
            var parts = [];
            resp.on('data', (chunk)=>{
                parts.push(chunk);
            });
            resp.on('end', ()=>{
                var body = Buffer.concat(parts);
                var respString = body.toString();
                var timestamp = Number(respString);
                this.log('resp', respString);
                if(timestamp){
                    this.emit('timestamp', timestamp);
                    this.gotTimestamp = true;
                    this._onSuccess();
                }else if('81ce4465-7167-4dcb-835b-dcc9e44c112a' === respString){
                    this._onSuccess();
                }
            });
        });
        this.timeoutTimer = setTimeout(()=>{
            req.abort();
            this.log('timeout');
            this._onFail();
        }, this.timeout);
        req.on('error', (err)=>{
            this.log(err);
            this._onFail();
        });
        req.end();
    }

    start(){
        this.quit = false;
        this.gotTimestamp = false;
        this._check();
    }

    stop(){
        this.quit = true;
    }
};

module.exports = InternetMonitor;