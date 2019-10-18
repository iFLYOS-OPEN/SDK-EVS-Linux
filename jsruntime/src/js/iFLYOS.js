var Log = require('console').Log;
var EventEmitter = require('events').EventEmitter;
var util = require('util');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('iFLYOS'));

class Router{
    constructor(props) {
        native.bindJSRouter(this);
        this._events = {__test:"abcd"};
    }

    once(name, handler){
        var f = function() {
            // here `this` is this not global, because EventEmitter binds event object
            // for this when it calls back the handler.
            this.removeListener(f.name, f);
            //log("apply in once handler", arguments);
            f.handler.apply(this, arguments);
        };

        f.name = name;
        f.handler = handler;

        this.on(name, f);
    }

    on(name, handler) {
        if (!util.isFunction(handler)) {
            throw new TypeError('listener must be a function');
        }

        if (!this._events) {
            this._events = {};
        }

        if(!this._events[name]){
            log("native observe", name);
            native.observe(name);
        }

        if (!this._events[name]) {
            this._events[name] = [];
        }

        this._events[name].push(handler);
        //log("after 'on'", this._events[name][0]);

        return this;
    }

    removeListener(name, handler) {
        var list = this._events[name];
        if (Array.isArray(list)) {
            for (var i = list.length - 1; i >= 0; --i) {
                if (list[i] == handler ||
                    (list[i].handler && list[i].handler == handler)) {
                    list.splice(i, 1);
                    if (!list.length) {
                        delete this._events[name];
                    }
                    break;
                }
            }
        }
        if(!this._events[name]){
            native.unobserve(name);
        }
    }

    sendIVS(name, params) {
        this.send("ivs", name, params);
    }

    send(dest, name, params){
        if(params === undefined){
            params = [];
        }
        if(util.isString(params)){
            params = [params];
        }
        if(!util.isArray(params)){
            throw new TypeError('params must be array');
        }
        params.forEach(function(p){
            if(!util.isString(p))
                throw new TypeError('param must be string')
        });
        log("to send", name, params, "to", dest);
        native.sendMessage(dest, name, params);
    }

    broadcast(name, params) {
        this.send("router", name, params);
    }

    _nativeCallback(){
        if(!arguments[0])
            throw "message no name";
        log("_nativeCallback", arguments[0]);
        var name = arguments[0];
        var params = Array.prototype.slice.call(arguments, 1);
        //setTimeout(()=>this.emit(name, params));
        this.emit(name, params);
    }

    emit(name, params){
        log('emit', name);
        if (!this._events) {
            this._events = {};
        }

        var handlers = this._events[name];
        if (util.isArray(handlers)) {
            handlers = handlers.slice();
            for (var i = 0; i < handlers.length; ++i) {
                //log("to apply in emit with params", params, handlers[i]);
                handlers[i].apply(this, params);
            }
            return true;
        }

        return false;
    }
}

class R328Source{
    constructor(channelNum){
        native.R328Source_new.bind(this)(channelNum);
        this.sinks = [];
    }
    connect(sink){
        var err = native.R328Source_connect.bind(this)(sink);
        if(err) throw err;
        this.sinks.push(sink);
    }
    start(){
        native.R328Source_start.bind(this)();
    }
    stop(){
        native.R328Source_stop.bind(this)();
    }
}
class ALSASource{
    constructor(deviceName, channelNum, periodTime, is32bit){
        native.ALSASource_new.bind(this)(deviceName, channelNum, periodTime, is32bit);
        this.sinks = [];
    }
    static cset(p1, p2){
        log("cset",p1,p2);
        native.ALSA_cset(p1, p2);
    }
    connect(sink){
        var err = native.ALSASource_connect.bind(this)(sink);
        if(err) throw err;
        this.sinks.push(sink);
    }
    start(){
        native.ALSASource_start.bind(this)();
    }
    stop(){
        native.ALSASource_stop.bind(this)();
    }
}

class Snowboy extends EventEmitter{
    constructor(modelPath, resPath){
        super();
        native.Snowboy_new.bind(this)(modelPath, resPath);
    }
    start(){
        native.Snowboy_start.bind(this)();
    }
    stop(){
        native.Snowboy_stop.bind(this)();
    }
    _nativeWakeup_callback(params){
        this.emit('wakeup', params);
    }
}

class iFlytekCAE extends EventEmitter{
    constructor(resPath, channelNum) {
        super();
        native.iFlytekCAE_new.bind(this)(resPath, channelNum);
    }
    start(){
        native.iFlytekCAE_start.bind(this)();
    }
    stop(){
        native.iFlytekCAE_stop.bind(this)();
    }
    connect(sink){
        native.iFlytekCAE_connect.bind(this)(sink);
    }
    _nativeWakeup_callback(params){
        //log('_nativeWakeup_callback', JSON.stringify(params));
        this.emit('wakeup', params);
    }
    reload(filePath){
        native.iFlytekCAE_reload.bind(this)(filePath);
    }
    auth(token){
        native.iFlytekCAE_auth.bind(this)(token);
    }
}

class SharedStreamSink{
    constructor(bufferSize){
        bufferSize = bufferSize || (16000 * 2 * 5);//5s mono
        log("new SharedStreamSink", bufferSize);
        native.SharedStreamSink_new.bind(this)(bufferSize);
    }
    createReader(){
        return native.SharedStreamSink_createReader.bind(this)();
    }
}

class FileWriterSink{
    constructor(filePath) {
        log('new FileWriterSink');
        native.FileWriterSink_new.bind(this)(filePath);
    }
    start(){
        native.FileWriterSink_start.bind(this)();
    }
    stop(){
        native.FileWriterSink_stop.bind(this)();
    }
}

class SonicDecoder{

}

exports.initIPC = function(){
    exports.router = new Router();
    return exports.router;
};

exports.ALSASource = ALSASource;
exports.R328Source = R328Source;
exports.iFlytekCAE = iFlytekCAE;
exports.Snowboy = Snowboy;
exports.FileWriterSink = FileWriterSink;
exports.SonicDecoder = SonicDecoder;
exports.CAE = iFlytekCAE;
exports.platform = native.platform;
exports.captureChannelNum = native.captureChannelNum;
exports.SharedStreamSink = SharedStreamSink;
