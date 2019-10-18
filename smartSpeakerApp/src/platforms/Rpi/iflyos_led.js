var Log = require('console').Log;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('LEDControl'));


class LEDControl  {
    constructor(cb) {
        setTimeout(cb, 100);
    }
    /**
     * powering on
     */
    startup(){
        log('start up');
        return Promise.resolve();
    }

    /**
     * power on finish
     */
    startupDone(){
        log('start up done');
        return Promise.resolve();
    }

    /**
     * pop to stable LED status
     */
    back2Idle(){
        log('back to idle');
        this.onIdle && this.onIdle();
    }

    _noInternet(){
        return Promise.resolve();
    }

    /**
     * stable status become 'no internet connection'
     */
    noInternet(){
        log('noInternet()');
        this.onIdle = this._noInternet.bind(this);
        this._noInternet();
    }

    _normal(){
        return Promise.resolve();
    }

    /**
     * stable status become 'normal', meaning can wake up for voice recognition
     */
    normal(){
        log('normal()');
        this.onIdle = this._normal.bind(this);
        return this._normal();
    }

    /**
     * temporary status 'wakeup'
     * @param angle 0~360
     */
    wakeup(angle){
        this.angle = angle % 360;
        log('wakeup', angle);
        return Promise.resolve();
    }

    /**
     * temporary status 'thinking'
     * @param angle 0~360, or the angle in last wake up
     */
    thinking(angle){
        if(angle == undefined) {
            angle = this.angle;
        }
        log('thinking', angle);
        return Promise.resolve();
    }

    /**
     * evs speaking tts
     */
    speaking(){
        log('speaking');
        return Promise.resolve();
    }

    /**
     * evs listening for recognize
     */
    listening(angle){
        if(angle == undefined) {
            angle = this.angle;
        }
        log('listening', angle);
        return Promise.resolve();
    }

    alert(){
        log('alart');
        return Promise.resolve();
    }

    configNet(){
        log('config net');
        return Promise.resolve();
    }
}

module.exports.LEDControl = LEDControl;