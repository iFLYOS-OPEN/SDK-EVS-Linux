var console = require("console");
var Log = console.Log;
var EventEmitter = require('events');
var EasyCurl = require('curl');

var DollMgr = require('./platforms/' + iflyosPlatform + '/dollMgr').DollMgr;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ModuleDoll'));

class EVSModuleDoll extends EventEmitter {
    constructor(moduleStartup, moduleConfig, moduleInteraction, cxtMgr, authMgr, soundPlayer, ledCtrl) {
        super();

        this.moduleInteraction = moduleInteraction;
        this.soundPlayer = soundPlayer;
        this.ledCtrl = ledCtrl;
        this.dollMgr = new DollMgr();

        moduleStartup.once('ready', this._onceStartupReady.bind(this));
        moduleConfig.on('config_start', this._onConfigStart.bind(this));
        moduleConfig.on('config_end', this._onConfigEnd.bind(this));
        moduleInteraction.on('client', this._onClientStatus.bind(this));
        cxtMgr.addContextProvider(this._getContext.bind(this));
        authMgr.on('access_token', this._onAccessToken.bind(this));

        this.dollMgr.on('init', this._onDollInit.bind(this));
        this.dollMgr.on('preparing', this._onDollPerparing.bind(this));
        this.dollMgr.on('cancel', this._onDollCancel.bind(this));
        this.dollMgr.on('doll_set', this._onDollSet.bind(this));
        this.dollMgr.on('doll_unset', this._onDollUnset.bind(this));

        this.authStatus = null;
        this.dollStatus = 'unplugged';

        this.isConnectEVS = false;
        this.isFirstConnect = true;
        this.isEnabled = false;
        this.token = null;

        this.dollId = null;
        this.hiSoundFile = null;
        this.byeSoundFile = null;
        this.caeResFile = null;
    }

    _onceStartupReady(status) {
        this.authStatus = status;
        this.dollMgr.start();
    }

    _getContext() {
        var cxt = {
            interceptor: {
                adam: {
                    doll_id: 'null'
                }
            }
        };

        if (this.dollStatus === 'ready' && this.isEnabled) {
            cxt.interceptor.adam.doll_id = this.dollId.toString();
        }
        return cxt;
    }

    _onAccessToken(token) {
        this.token = token;
        this.dollMgr.updateToken(token);
    }

    _onClientStatus(status) {
        if (status === 'connected') {
            this.isConnectEVS = true;
            if (this.isFirstConnect) {
                this.isFirstConnect = false;
                if (this.dollStatus === 'unplugged') {
                    this.soundPlayer.play('tts_我已联网');
                } else if (this.dollStatus === 'ready') {
                    this.isEnabled = true;
                    this.moduleInteraction.reloadRes(this.caeResFile);
                    this.soundPlayer.playFile(this.hiSoundFile, () => {
                        this.ledCtrl.normal();
                    });
                    //this._dollReport();
                } else if (this.dollStatus === 'plugged') {
                    this.moduleInteraction.enableWakeup(false);
                    this.soundPlayer.play('tts_我正在准备');
                    this.ledCtrl.downloading();
                    this.dollMgr.startDownloadRes();
                }
            }
        } else {
            this.isConnectEVS = false;
        }
    }

    _onDollInit(info) {
        if (info === 'not_present') {
            //没有玩偶
            log('doll init: not_present');
            this.dollStatus = 'unplugged';
        } else if (info && info.hiSoundFile) {
            //有玩偶且资源已经存在
            log('doll init:', info.hiSoundFile);
            this.dollId = info.dollId;
            this.hiSoundFile = info.hiSoundFile;
            this.byeSoundFile = info.byeSoundFile;
            this.caeResFile = info.caeResFile;
            this.dollStatus = 'ready';
        } else {
            //有玩偶但资源不存在
            log('doll init: need download res');
            this.dollStatus = 'plugged';
        }
    }

    _onDollPerparing() {
        log('doll preparing');
        if (this.dollStatus !== 'plugged') {
            this.dollStatus = 'plugged';
            if (this.isConnectEVS) {
                this.moduleInteraction.enableWakeup(false);
                this.soundPlayer.play('tts_我正在准备');
                this.ledCtrl.downloading();
            }
        }
    }

    _onDollCancel(err) {
        log('doll cancel:', err);
        //this.ledCtrl.back2Idle();
        this.moduleInteraction.enableWakeup(true);
        if (!this.isConfiging) {
            if (this.authStatus === 'unauthorized') {
                this.soundPlayer.play('tts_欢迎使用');
            } else if (this.authStatus === 'auth_fail') {
                this.soundPlayer.play('tts_网络配置失败');
            } else if (this.authStatus === 'auth_unknown') {
                this.soundPlayer.play('tts_已失去网络连接');
            } else if (this.authStatus === 'auth_expired') {
                this.soundPlayer.play('tts_登录状态失效');
            } else if (!this.isConnectEVS) {
                this.soundPlayer.play('tts_已失去网络连接');
            } else if (err === 'doll' || err === 'server') {
                this.dollStatus = 'doll_error';
                this.soundPlayer.play('tts_无法识别，请重新放置人偶');
            } else if (err === 'network') {
                this.dollStatus = 'network_error';
                this.soundPlayer.play('tts_网络有点问题，请稍后重试');
            } else {
                this.dollStatus = 'error';
                log('unknown error type:', err);
            }
        }
    }

    _onDollSet(info) {
        log('doll set');
        this.dollId = info.dollId;
        this.hiSoundFile = info.hiSoundFile;
        this.byeSoundFile = info.byeSoundFile;
        this.caeResFile = info.caeResFile;
        this.dollStatus = 'ready';
        if (!this.isConfiging) {
            this.isEnabled = this.isConnectEVS;
            if (this.isConnectEVS) {
                this.moduleInteraction.enableWakeup(true);
                this.ledCtrl.dollSet();
                this.moduleInteraction.stopAlert();
                this.moduleInteraction.stopAudio();
                this.moduleInteraction.reloadRes(this.caeResFile);
                this.soundPlayer.playFile(info.hiSoundFile, () => {
                    this.ledCtrl.normal();
                });
                //this._dollReport();
            } else {
                this._onDollCancel();
            }
        }
    }

    _onDollUnset(info) {
        log('doll unset');
        this.dollStatus = 'unplugged';

        if (this.isEnabled) {
            this.moduleInteraction.reloadRes();
            //this._dollReport();
        }

        if (!this.isConfiging && this.isEnabled) {
            this.moduleInteraction.enableWakeup(true);
            this.moduleInteraction.stopAlert();
            this.moduleInteraction.stopAudio();
            this.soundPlayer.playFile(info.byeSoundFile);
        }
    }

    _onConfigStart() {
        this.isConfiging = true;
        this.isFirstConnect = true;
        this.authStatus = 'unauthorized';
    }

    _onConfigEnd(err) {
        this.isConfiging = false;
        this.authStatus = err ? 'auth_fail' : 'authorized';
    }

    // _dollReport() {
    //     var reportUrl;
    //     if (this.dollStatus === 'ready') {
    //         reportUrl = 'https://adam.iflyos.cn/doll_ready';
    //     } else if (this.dollStatus === 'unplugged') {
    //         reportUrl = 'https://adam.iflyos.cn/doll_remove';
    //     } else {
    //         return;
    //     }
    //
    //     log('report url:', reportUrl);
    //     var curl = new EasyCurl();
    //     curl.addHeader('Accept: text/plain');
    //     curl.addHeader('Content-Type: text/plain');
    //     curl.addHeader('Authorization: Bearer ' + this.token);
    //     curl.setOpt(EasyCurl.CURLOPT_URL, reportUrl);
    //     curl.setOpt(EasyCurl.CURLOPT_TIMEOUT, 10);
    //     curl.setOpt(EasyCurl.CURLOPT_POST, 1);
    //     curl.perform((err) => {
    //         if (err) {
    //             log(reportUrl, err);
    //         }
    //     });
    // }
}

module.exports.EVSModuleDoll = EVSModuleDoll;