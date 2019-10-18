var console = require("console");
var Log = console.Log;

var iFlyosUtil = require('iflyos_util');
var Voice = require('./audio.js').Voice;
var EVSContextMgr = require('./evsContextMgr').EVSContextMgr;
var EVSClient = require('./evsClient').EVSClient;
var EVSCapSystem = require('./evsCapSystem').EVSCapSystem;
var EVSCapSpeaker = require('./evsCapSpeaker').EVSCapSpeaker;
var EVSCapRecognizer = require('./evsCapRecognizer').EVSCapRecognizer;
var EVSCapInterceptor = require('./evsCapInterceptor').EVSCapInterceptor;
var EVSCapAudioPlayer = require('./evsCapAudioPlayer').EVSCapAudioPlayer;

var EVSTTSPlayer = require('./evsTTSPlayer').EVSTTSPlayer;
var EVSAlertPlayer = require('./evsAlertPlayer').EVSAlertPlayer;
var EVSAudioPlayer = require('./evsAudioPlayer').EVSAudioPlayer;
var EVSEffectPlayer = require('./platforms/' + iflyosPlatform + '/evsEffectPlayer').EVSEffectPlayer;

var EVSButtonControl = require('./platforms/' + iflyosPlatform + '/evsButtonControl').EVSButtonControl;

var ChannelType = require('./evsAudioMgr').ChannelType;
var FocusState = require('./evsAudioMgr').FocusState;

var DollMgr = require('./platforms/' + iflyosPlatform + '/dollMgr').DollMgr;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('InteractionMgr'));

class EVSModuleWakeup {
    constructor(voice, soundPlayer, moduleStartup, moduleConfig) {
        this.soundPlayer = soundPlayer;

        voice.on('wakeup', this._onWakeup.bind(this));
        moduleStartup.on('ready', this._onModuleStartupReady.bind(this));
        moduleConfig.on('config_start', this._onModuleConfigStart.bind(this));
        moduleConfig.on('config_end', this._onModuleConfigEnd.bind(this));

        this.authStatus = 0;
        this.isStartupReady = false;
        this.isConfiging = false;
        this.isMicDisabled = false;
        this.isConnectEVS = false;
    }

    mute() {
        this.isMicDisabled = !this.isMicDisabled;
    }

    isMuted() {
        return this.isMicDisabled;
    }

    setAuthStatus(status) {
        this.authStatus = status;
    }

    _onWakeup(params) {
        log('wakeup', JSON.stringify(params));
        if (this.isStartupReady) {
            this.soundPlayer.play('tts_我还在开机，请稍后再唤醒我吧');
        } else if (this.isConfiging) {
            log('ignore wakeup for wifi configuring and authorizing');
        } else if (this.authStatus === 'unauthorized') {
            this.soundPlayer.play('tts_欢迎使用');
        } else if (this.authStatus === 'auth_expired') {
            this.soundPlayer.play('tts_登录状态失效，请打开APP重新登录');
        } else if (this.authStatus === 'auth_fail') {
            this.soundPlayer.play('tts_网络配置失败');
        } else if (this.isMicDisabled) {
            log('mic has been disabled');
        } else if (!this.isConnectEVS) {
            this.soundPlayer.play('tts_已失去网络连接，请打开APP检查网络配置');
        } else {
            this.emit('wakeup', params);
        }
    }

    _onModuleStartupReady(status) {
        this.isStartupReady = true;
        this.authStatus = status;
    }

    _onModuleConfigStart() {
        this.isConfiging = true;
        this.isMicDisabled = false;
        this.authStatus = 'unauthorized';
    }

    _onModuleConfigEnd(err) {
        this.isConfiging = 'false';
        this.authStatus = err ? 'auth_fail' : 'authorized';
    }
}

module.exports.EVSModuleWakeup = EVSModuleWakeup;