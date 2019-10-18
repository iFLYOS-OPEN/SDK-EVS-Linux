var iflyos = require('iflyos');
var EventEmitter = require('events');
var Log = require('console').Log;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('voice'));

class Voice extends EventEmitter{
    constructor(props) {
        super(props);
        this.props = props;
        log('creation prop:', props, 'record channels count:', iflyos.captureChannelNum);
        this.streamReader = undefined;
        this.streamReaderInterval = undefined;
        if(iflyosPlatform === 'LinuxHost' || iflyosPlatform === 'Rpi') {
            if (!props || !props.snowboyModel || !props.snowboyRes) {
                throw "snowboyModel or snowboyRes not set";
            }
            this.mic = new iflyos.ALSASource(props.deviceName || "hw:1,0", 1, 20000, false);
            this.snowboy = new iflyos.Snowboy(props.snowboyModel, props.snowboyRes);
            this.stream = new iflyos.SharedStreamSink();
            var err = this.mic.connect(this.snowboy);
            if (err) throw err;
            err = this.mic.connect(this.stream);
            if (err) throw err;
            this.snowboy.on('wakeup', () => {
                this.emit('wakeup');
            });
        }else if(iflyosPlatform === "A113X") {
            if (!props || !props.caeResPath) {
                throw "caeResPath not set";
            }
            if(iflyosProduct == 'adam' || iflyosProduct == 'adam-edu'){
                console.log('use 32 bit record for hlw');
                this.mic = new iflyos.ALSASource(props.deviceName || "hw:0,3", iflyos.captureChannelNum, 8000, true);
            }else {
                this.mic = new iflyos.ALSASource(props.deviceName || "hw:0,3", iflyos.captureChannelNum, 8000, false);
            }
            this.cae = new iflyos.CAE(props.caeResPath, iflyos.captureChannelNum);
            this.stream = new iflyos.SharedStreamSink(1024 * 32 * 10);//10s buffer
            log("to connect cae to mic");
            var err = this.mic.connect(this.cae);
            if (err) throw err;
            log("to connect stream to cae");
            err = this.cae.connect(this.stream);
            if (err) throw err;
            log("to register wakeup listener to cae");
            this.cae.on('wakeup', (params) => {
                this.emit('wakeup', params);
            });
        }else if(iflyosPlatform === "MTK8516") {
            if (!props || !props.caeResPath) {
                throw "caeResPath not set";
            }
            var micGain = "9";
            var refGain = "9";
            iflyos.ALSASource.cset("name='PGA1_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA2_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA3_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA4_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA5_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA6_setting'", micGain);
            iflyos.ALSASource.cset("name='PGA7_setting'", refGain);
            iflyos.ALSASource.cset("name='PGA8_setting'", refGain);
            this.mic = new iflyos.ALSASource(props.deviceName || "hw:0,1", iflyos.captureChannelNum, 8000, false);
            this.cae = new iflyos.CAE(props.caeResPath, iflyos.captureChannelNum);
            this.stream = new iflyos.SharedStreamSink();
            log("to connect cae to mic");
            var err = this.mic.connect(this.cae);
            if (err) throw err;
            log("to connect stream to cae");
            err = this.cae.connect(this.stream);
            if (err) throw err;
            log("to register wakeup listener to cae");
            this.cae.on('wakeup', () => {
                this.emit('wakeup');
            });
        }else if(iflyosPlatform == 'R328'){
            if (!props || !props.caeResPath) {
                throw "caeResPath not set";
            }
            this.mic = new iflyos.R328Source(iflyos.captureChannelNum);
            this.cae = new iflyos.CAE(props.caeResPath, iflyos.captureChannelNum);
            this.stream = new iflyos.SharedStreamSink();
            log("to connect cae to mic");
            var err = this.mic.connect(this.cae);
            if (err) throw err;
            log("to connect stream to cae");
            err = this.cae.connect(this.stream);
            if (err) throw err;
            log("to register wakeup listener to cae");
            this.cae.on('wakeup', () => {
                this.emit('wakeup');
            });
        }else{
            throw "platform" + iflyosPlatform + "not implemented now"
        }
    }
    realodCaeRes(file){
        file = file || this.props.caeResPath;
        log('reload cae resource file from', file);
        this.cae.reload(file);
    }
    enableCapture(enable){
        log("enableCapture", enable);
        if(enable) return this.mic.start();
        return this.mic.stop();
    }
    enableWakeup(enable){
        log("enableWakeup", enable);
        if(this["snowboy"]) {
            if (enable)
                return this.snowboy.start();
            else
                return this.snowboy.stop();
        }
        else {
            if (enable)
                return this.cae.start()
            else
                return this.cae.stop();
        }
    }
    enableStreamPuller(enable){
        log("enableStreamPuller", enable);
        if(!enable && this.streamReader) {
            log("stop voice pull interval, read pcm:", this.pullCount);
            clearInterval(this.streamReaderInterval);
            this.streamReader.close();
            this.streamReader = undefined;//let GC do destory
        } else if (enable && !this.streamReader) {
            log("start voice pull interval");
            this.streamReader = this.stream.createReader();
            this.pullCount = 0;
            this.streamReaderInterval = setInterval(()=>
                {
                    var buf = this.streamReader.read();
                    //log('pull voice', buf.length);
                    if(buf.length > 0){
                        this.pullCount += buf.length;
                        this.emit('voice', buf);
                    }
                }
            , 40);//not stress js scheduler
        }
    }

    caeAuth(token){
        if(this.cae)
            this.cae.auth(token);
    }

    platform(){
        return iflyosPlatform;
    }
}

module.exports.Voice = Voice;
