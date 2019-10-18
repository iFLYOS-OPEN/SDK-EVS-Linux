var Log = require("console").Log;
var EventEmitter = require('events');
var iFlyosUtil = require('iflyos_util');

var EVSOTAMgr = require('./platforms/' + iflyosPlatform + '/evsOTAMgr').EVSOTAMgr;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ModuleStartup'));

/**
 * EVS program startup manager implementation
 * @class
 */
class EVSModuleStartup extends EventEmitter {
    /**
     * Create EVSNetworkMgr instance
     * @param {json} cfg - configurationis
     */
    constructor(cfg, authMgr, netMonitor, soundPlayer) {
        super();

        this.authMgr = authMgr;
        this.otaMgr = new EVSOTAMgr(cfg);
        this.soundPlayer = soundPlayer;

        this.authTimer = null;
        this.status = 'unauthorized';

        netMonitor.once('ok', this._onceInternetOk.bind(this));
        netMonitor.on('timestamp', this._onTimestamp.bind(this));
    }

    start() {
        Promise.resolve().then(() => {
            return this._checkOta();
        }).then(() => {
            return this._checkAuth();
        }).then(() => {
            log('startup ready with auth status:', this.status);
            this.emit('ready', this.status);
        });
    }

    _checkOta() {
        return new Promise((resolve) => {
            this.otaMgr.once('status', (status) => {
                if (status === 'updating') {
                    this.soundPlayer.play('tts_固件升级中1');
                } else if (status === 'ota_ready') {
                    this.soundPlayer.play('tts_升级成功', () => {
                        resolve();
                    });
                } else if (status === 'ready') {
                    resolve();
                }
            });

            this.otaMgr.start();
        });
    }

    _checkAuth() {
        return new Promise((resolve) => {
            var onceAuthFail = (err) => {
                log('authorize fail:', err);
                this._clearTimer();
                if (err.message === 'invalid_refresh_token') {
                    this.soundPlayer.play('tts_登录状态失效');
                    this.status = 'auth_expired';
                    resolve();
                } else {
                    this.soundPlayer.play('tts_已失去网络连接');
                    this.status = 'auth_unknown';
                    resolve();
                }
            };

            var onceAuthSuccess = (token) => {
                log('auth success:', token);
                this._clearTimer();
                this.status = 'authorized';
                resolve();
            };

            this.authMgr.once('fail', onceAuthFail);
            this.authMgr.once('access_token', onceAuthSuccess);

            this.authMgr.getRefreshToken((token) => {
                if (!token) {
                    this.soundPlayer.play('tts_欢迎使用');
                    resolve();
                } else {
                    this.authMgr.authWithRefreshToken(token);
                    this.authTimer = setTimeout(() => {
                        this.authMgr.removeListener('access_token', onceAuthSuccess);
                        onceAuthFail(new Error('network_error'));
                    }, 30000);
                }
            });
        });
    }

    _onTimestamp(timestamp) {
        timestamp += 10 * 3600;
        var date = new Date(timestamp * 1000).toDateString();
        var time = new Date(timestamp * 1000).toTimeString().substr(0, 8);
        iFlyosUtil.system('date -s "' + date + ' ' + time + '"');
        log('update system time:', date, time);
    }

    _onceInternetOk() {
        this.otaMgr.otaCheck();
    }


    _clearTimer() {
        if (this.authTimer) {
            clearTimeout(this.authTimer);
            this.authTimer = null;
        }
    }
}

module.exports.EVSModuleStartup = EVSModuleStartup;
