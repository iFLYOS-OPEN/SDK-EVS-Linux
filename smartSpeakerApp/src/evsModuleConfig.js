var Log = require("console").Log;
var EventEmitter = require('events');
var EVSNetworkMgr = require('./evsNetworkMgr').EVSNetworkMgr;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ModuleConfig'));

/**
 * EVS program startup manager implementation
 * @class
 */
class EVSModuleConfig extends EventEmitter  {
    /**
     * Create EVSNetworkMgr instance
     * @param {EVSNetworkMgr} networkMgr
     * @param {EVSAuthorizeMgr} authMgr
     * @param {netMonitor} netMonitor
     * @param {EVSSoundPlayer} soundPlayer
     */
    constructor(cfg, authMgr, soundPlayer) {
        super();

        this.networkMgr = new EVSNetworkMgr(cfg);
        this.authMgr = authMgr;
        this.soundPlayer = soundPlayer;
        this.authTimer = null;
        this.isConfiging = false;

        this.networkMgr.on('status', this._onNetworkStatus.bind(this));
        this.networkMgr.on('fail', this._onNetworkFail.bind(this));
        this.networkMgr.on('ok', this._onNetworkSuccess.bind(this));
        this.authMgr.on('fail', this._onAuthFail.bind(this));
        this.authMgr.on('access_token', this._onAuthSuccess.bind(this));
    }

    config() {
        this.isConfiging = true;
        this.authMgr.reset();
        this.networkMgr.start();
        this._clearTimer();
        this.emit('config_start');
    }

    _onNetworkStatus(status) {
        if (this.isConfiging) {
            if (status === 'ready') {
                this.soundPlayer.play('tts_进入网络配置模式');
            } else if (status === 'accept') {
                this.soundPlayer.play('tts_蓝牙已连接');
            } else if (status === 'config') {
                this.soundPlayer.play('tts_收到密码，正在努力联网');
            }
        }
    }

    _onNetworkFail(err) {
        if (this.isConfiging) {
            log('network fail:', err);
            if (err.message === 'accept_timeout' || err.message === 'data_timeout') {
                this.soundPlayer.play('tts_联网超时，请重试');
            } else if (err.message === 'disconnect') {
                this.soundPlayer.play('tts_蓝牙已断开');
            } else if (err.message === 'wrong_data' || err.message === 'config_fail') {
                this.soundPlayer.play('tts_WiFi密码不对');
            } else {
                this.soundPlayer.play('tts_联网失败，请确认WiFi是否可用');
            }

            this.networkMgr.stop();
            this.isConfiging = false;
            this.emit('config_end', err);
        }
    }

    _onNetworkSuccess(code) {
        if (this.isConfiging) {
            log('network success:', code);
            this.networkMgr.stop();

            this.authMgr.authWithCode(code);
            this.authTimer = setTimeout(() => {
                this._onAuthFail(new Error('network_error'));
            }, 30000);
        }
    }

    _onAuthFail(err) {
        if (this.isConfiging) {
            log('authorize fail:', err);
            this.authMgr.reset();
            this._clearTimer();

            if (err.message === 'invalid_refresh_token') {
                this.soundPlayer.play('tts_登录状态失效')
            } else {
                this.soundPlayer.play('tts_网络配置失败');
            }

            this.isConfiging = false;
            this.emit('config_end', err);
        }
    }

    _onAuthSuccess(token) {
        if (this.isConfiging) {
            log('auth success:', token);
            this._clearTimer();
            this.isConfiging = false;
            this.emit('config_end');
        }
    }

    _clearTimer() {
        if (this.authTimer) {
            clearTimeout(this.authTimer);
            this.authTimer = null;
        }
    }
}

module.exports.EVSModuleConfig = EVSModuleConfig;
