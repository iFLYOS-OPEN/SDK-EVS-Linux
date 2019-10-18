var console = require("console");
var Log = console.Log;
var EventEmitter = require('events');
var iFlyosUtil = require('iflyos_util');

var Voice = require('./audio.js').Voice;
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

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ModuleInteraction'));

class EVSModuleInteraction extends EventEmitter {
    constructor(cfg, moduleStartup, moduleConfig, cxtMgr, authMgr, audioMgr, netMonitor, soundPlayer) {
        super();

        this.moduleConfig = moduleConfig;
        this.cxtMgr = cxtMgr;
        this.authMgr = authMgr;
        this.netMonitor = netMonitor;
        this.soundPlayer = soundPlayer;

        // 初始化context管理模块，client管理模块
        var serverUrl = 'ws://ivs.iflyos.cn/embedded/v1';
        this.client = new EVSClient(serverUrl, cfg.deviceID, this.cxtMgr);

        // 初始化各个播放器
        this.ttsPlayer = new EVSTTSPlayer(audioMgr, this.client);
        this.alertPlayer = new EVSAlertPlayer(audioMgr, this.client);
        this.audioPlayer = new EVSAudioPlayer(audioMgr, this.client, this.cxtMgr);
        this.effectPlayer = new EVSEffectPlayer(cfg.soundDir);

        // 初始化录音模块
        this.voice = new Voice({
            snowboyRes: cfg.snowboyRes,
            snowboyModel: cfg.snowboyModel,
            voiceFIFOPath: cfg.ivsRecord,
            caeResPath: cfg.caeResPath
        });
        this.voice.caeAuth(cfg.deviceID);

        // 初始化端能力
        this.capSystem = new EVSCapSystem(this.client, this.cxtMgr);
        this.capInterceptor = new EVSCapInterceptor(this.client, this.cxtMgr, this.capSystem);
        this.capSpeaker = new EVSCapSpeaker(this.client, this.cxtMgr, this.capSystem);
        this.capRecognizer = new EVSCapRecognizer(this.client, this.cxtMgr, this.capSystem, audioMgr, this.voice);
        this.capAudioPlayer = new EVSCapAudioPlayer(this.client, this.capRecognizer,
            this.ttsPlayer, this.alertPlayer, this.audioPlayer);

        // 初始化按键控制模块
        this.btnCtrl = new EVSButtonControl();

        this.capSpeaker.addAudioPlayer(this.soundPlayer);
        this.capSpeaker.addAudioPlayer(this.ttsPlayer);
        this.capSpeaker.addAudioPlayer(this.alertPlayer);
        this.capSpeaker.addAudioPlayer(this.audioPlayer);
        this.capSpeaker.addAudioPlayer(this.effectPlayer);

        this.voice.enableWakeup(true);
        this.voice.enableCapture(true);

        moduleStartup.once('ready', this._onceStartupReady.bind(this));
        moduleConfig.on('config_start', this._onConfigStart.bind(this));
        moduleConfig.on('config_end', this._onConfigEnd.bind(this));

        this.voice.on('wakeup', this._onWakeup.bind(this));
        this.authMgr.once('access_token', this._onceAccessToken.bind(this));

        this.btnCtrl.on('PLAY_PAUSE', this._onPlayPause.bind(this));
        this.btnCtrl.on('VOL_DOWN', this._onVolumeDown.bind(this));
        this.btnCtrl.on('VOL_UP', this._onVolumeUp.bind(this));
        this.btnCtrl.on('VOL_MUTE', this._onVolumeMute.bind(this));
        this.btnCtrl.on('NET_CFG', this._onConfigWifi.bind(this));
        this.btnCtrl.on('DEV_MODE', this._onDevelopMode.bind(this));
        this.btnCtrl.on('RESTORE', this._onRestore.bind(this));

        audioMgr.on(ChannelType.TTS, (state) => {
            this.isTTSPlaying = !(state === FocusState.NONE);
        });
        audioMgr.on(ChannelType.ALERT, (state) => {
            this.isAlertPlaying = !(state === FocusState.NONE);
        });
        audioMgr.on(ChannelType.CONTENT, (state) => {
            this.isAudioPlaying = !(state === FocusState.NONE);
        });

        this.authStatus = 0;
        this.isStartupReady = false;
        this.isConfiging = false;
        this.isConnectEVS = false;
        this.isFirstConnect = true;
        this.isMicDisabled = false;
        this.isTTSPlaying = false;
        this.isAlertPlaying = false;
        this.isAudioPlaying = false;
    }

    reloadRes(filePath) {
        this.voice.realodCaeRes(filePath);
    }

    stopAlert() {
        this.alertPlayer.stop();
    }

    stopAudio() {
        this.audioPlayer.stop();
    }

    enableWakeup(enable) {
        this.voice.enableWakeup(enable);
    }

    _onceStartupReady(status) {
        this.isStartupReady = true;
        this.authStatus = status;

        this.authMgr.once('fail', (err) => {
            if (err.message === 'invalid_refresh_token') {
                this.soundPlayer.play('tts_登录状态失效');
                this.authStatus = 'auth_expired';
            }
        });
    }

    _onConfigStart() {
        this.isConfiging = true;
        this.isFirstConnect = true;
        this.isMicDisabled = false;
        this.authStatus = 'unauthorized';
        this.client.stop();
    }

    _onConfigEnd(err) {
        this.isConfiging = false;
        this.authStatus = err ? 'auth_fail' : 'authorized';
    }

    // 开机后第一次获取到access token
    _onceAccessToken(token) {
        // 完成client初始化工作
        this.client.once('ready', () => {
            log('client is ready');
            this.client.startServerConnection();
        });

        // 完成和EVS服务器的连接
        this.client.on('connected', () => {
            log('client is connected');
            this.isConnectEVS = true;
            if (this.isFirstConnect) {
                // 启动后或者重新配网后首次连接服务器
                this.isFirstConnect = false;
                if (iflyosProduct !== 'adam') {
                    this.soundPlayer.play('tts_我已联网');
                }
            }
            this.emit('client', 'connected');
        });

        // 失去和EVS服务器的连接
        this.client.on('disconnected', () => {
            log('client is disconnected');
            this.isConnectEVS = false;
            this.emit('client', 'disconnected');
        });

        // 当设备被用户在app中删除时
        this.capSystem.on('device_removed', () => {
            this.authStatus = 'auth_expired';
            this.soundPlayer.play('tts_登录状态失效，请打开APP重新登录');
            this.client.stop();
        });

        // 旧的access token失效后，重新获取到新的并更新至client
        this.authMgr.on('access_token', (token) => {
            log('update access token with ', token);
            this.authStatus = 'authorized';
            this.client.updateToken(token);
            this.client.startServerConnection();
        });

        // 网络恢复时client重连
        this.netMonitor.on('ok', () => {
            log('network is ok');
            if (this.authStatus === 'authorized') {
                this.client.startServerConnection();
            }
        });

        // 网络断开时client停止
        this.netMonitor.on('fail', () => {
            log('network is fail, stop client');
            this.client.stop();
            if (this.isAudioPlaying) {
                this.soundPlayer.play('tts_已失去网络连接，请打开APP检查网络配置');
            }
        });

        this.capRecognizer.on('idle', () => this.emit('dialog', 'idle'));
        this.capRecognizer.on('listening', () => this.emit('dialog', 'listening'));
        this.capRecognizer.on('thinking', () => this.emit('dialog', 'thinking'));

        // 启动程序初始化工作
        this.client.updateToken(token);
        this.client.startLocalInit();
        this.authStatus = 'authorized';
    }

    _onWakeup(params) {
        log('wakeup', JSON.stringify(params));
        if (!this.isStartupReady) {
            this.soundPlayer.play('tts_我还在开机，请稍后再唤醒我吧');
        } else if (this.isConfiging) {
            log('ignore wakeup for wifi configuring and authorizing');
        } else if (this.isMicDisabled) {
            log('mic has been disabled');
        } else if (this.authStatus === 'unauthorized') {
            if (iflyosProduct === "rpi") {
                this.moduleConfig.config();
            } else {
                this.soundPlayer.play('tts_欢迎使用');
            }
        } else if (this.authStatus === 'auth_expired') {
            this.soundPlayer.play('tts_登录状态失效，请打开APP重新登录');
        } else if (this.authStatus === 'auth_fail') {
            this.soundPlayer.play('tts_网络配置失败');
        } else if (this.authStatus === 'auth_unknown') {
            this.soundPlayer.play('tts_已失去网络连接，请打开APP检查网络配置');
        } else if (!this.isConnectEVS) {
            this.soundPlayer.play('tts_已失去网络连接，请打开APP检查网络配置');
        } else {
            this.emit('wakeup', params);

            // if (this.isAlertPlaying) {
            //     log('alert is playing, stop alert first');
            //     this.alertPlayer.stop();
            // }

            if (this.soundPlayer.getVolume() === 0) {
                this.capSpeaker.updateVolume(10, true);
            }
            this.effectPlayer.play('音效_唤醒_1');
            this.capRecognizer.recognize();
        }
    }

    _onPlayPause() {
        log('BTN PLAY_PAUSE');
        if (this.isAlertPlaying) {
            this.alertPlayer.stop();
        } else if (this.isAudioPlaying) {
            this.audioPlayer.stop();
        } else {
            if (this.isTTSPlaying) {
                this.ttsPlayer.stop();
            }

            if (iflyosProduct === 'adam' || iflyosProduct === 'adam-edu') {
                // 由于亚当只有一个按键，所以在设备没有播放情况下，做麦克风禁用功能
                this._onVolumeMute();
            } else {
                this.capRecognizer.nlp('继续播放');
            }
        }
    }

    _onVolumeDown() {
        log('BTN VOL_DOWN');
        this.capSpeaker.adjustVolume(-10);
    }

    _onVolumeUp() {
        log('BTN VOL_UP');
        this.capSpeaker.adjustVolume(10);
    }

    _onVolumeMute() {
        log('BTN VOL_MUTE');
        if (!this.isConfiging) {
            this.isMicDisabled = !this.isMicDisabled;
            this.emit('mic', this.isMicDisabled);
            if (this.isMicDisabled) {
                this.soundPlayer.play('tts_麦克风已禁用');
            } else {
                this.soundPlayer.play('tts_麦克风已打开');
            }
        }
    }

    _onConfigWifi() {
        log('BTN NET_CFG');
        this.moduleConfig.config();
    }

    _onDevelopMode() {
        log('BTN DEV_MODE');
        if (this.isConfiging) {
            this.soundPlayer.play('tts_进入工厂模式');

            var adbCtrl = require('./adbController');
            adbCtrl.temporaryEnable();
        }
    }

    _onRestore() {
        log('BTN RESTORE');
        if (this.isConfiging) {
            this.soundPlayer.play('tts_恢复出厂设置', () => {
                iFlyosUtil.system('rm -rf /data/auth.db /data/doll /data/evs.log*');
                iFlyosUtil.system('sync');
                iFlyosUtil.system('reboot');
            });
        }
    }
}

module.exports.EVSModuleInteraction = EVSModuleInteraction;