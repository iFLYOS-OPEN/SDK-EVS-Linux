var console = require("console");
var Log = console.Log;

var ChannelType = require('./evsAudioMgr').ChannelType;
var FocusState = require('./evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ModuleLed'));

class EVSModuleLed {
    constructor(moduleStartup, moduleConfig, moduleInteraction, ledCtrl, audioMgr) {
        this.isConfiging = false;
        this.isFirstConnect = true;
        this.isConnectEVS = false;
        this.isTTSPlaying = false;
        this.isAlertPlaying = false;
        this.isMicDisabled = false;
        this.isWakeup = false;

        this.angle = null;
        this.ledStatus = null;
        this.ledCtrl = ledCtrl;

        moduleStartup.once('ready', this._onceStartupReady.bind(this));
        moduleConfig.on('config_start', this._onConfigStart.bind(this));
        moduleConfig.on('config_end', this._onConfigEnd.bind(this));
        moduleInteraction.on('client', this._onClientStatus.bind(this));
        moduleInteraction.on('dialog', this._onDialogStatus.bind(this));
        moduleInteraction.on('wakeup', this._onWakeup.bind(this));
        moduleInteraction.on('mic', this._onMicStatus.bind(this));

        audioMgr.on(ChannelType.AIP, this._onAIPFocusChange.bind(this));
        audioMgr.on(ChannelType.TTS, this._onTTSFocusChange.bind(this));
        audioMgr.on(ChannelType.ALERT, this._onAlertFocusChange.bind(this));

        this._setLed('startup');
    }

    _ledLogic() {
        if (this.isConfiging) {
            log('led for wifi configing');
            this._setLed('configing');
        } else if (this.isWakeup) {
            log('led for wakeup');
            this._setLed('wakeup');
            this.isWakeup = false;
        } else if (!this.isConnectEVS) {
            log('led for disconnected');
            this._setLed('disconnect');
        } else if (this.isFirstConnect) {
            log('led for first connect');
            this._setLed('startupDone');
        } else if (this.isMicDisabled) {
            log('led for mic disabled');
            this._setLed('disconnect')
        } else if (this.isTTSPlaying) {
            log('led for tts playing');
            this._setLed('speaking');
        } else if (this.isAlertPlaying) {
            log('led for alert playing');
            this._setLed('alerting');
        } else if (this.dialogStatus === 'thinking') {
            log('led for thinking');
            this._setLed('thinking');
        } else {
            log('led for normal');
            this._setLed('normal');
        }
    }

    _setLed(status) {
        if (status === 'wakeup' || this.ledStatus !== status) {
            this.ledStatus = status;
            if (this.ledStatus === 'startup') {
                this.ledCtrl.startup();
            } else if (this.ledStatus === 'startupDone') {
                this.ledCtrl.startupDone().then(() => this.ledCtrl.normal());
                this.ledStatus = 'normal';
            } else if (this.ledStatus === 'configing') {
                this.ledCtrl.configNet();
            } else if (this.ledStatus === 'wakeup') {
                this.ledCtrl.wakeup(this.angle);
            } else if (this.ledStatus === 'disconnect') {
                this.ledCtrl.noInternet();
            } else if (this.ledStatus === 'speaking') {
                this.ledCtrl.speaking();
            } else if (this.ledStatus === 'alerting') {
                this.ledCtrl.alert();
            } else if (this.ledStatus === 'thinking') {
                this.ledCtrl.thinking();
            } else if (this.ledStatus === 'normal') {
                this.ledCtrl.normal();
            }
        }
    }

    _onceStartupReady(status) {
        if (status !== 'authorized') {
            this._ledLogic();
        }
    }

    _onConfigStart() {
        this.isConfiging = true;
        this.isFirstConnect = true;
        this.isMicDisabled = false;
        this._ledLogic();
    }

    _onConfigEnd(err) {
        this.isConfiging = false;
        if (err) {
            this._ledLogic();
        }
    }

    _onClientStatus(status) {
        if (status === 'connected') {
            this.isConnectEVS = true;
            this._ledLogic();

            if (this.isFirstConnect) {
                this.isFirstConnect = false;
            }
        } else {
            this.isConnectEVS = false;
            this._ledLogic();
        }
    }

    _onDialogStatus(status) {
        if (status !== 'listening') {
            this.dialogStatus = status;
            this._ledLogic();
        }
    }

    _onWakeup(params) {
        this.isWakeup = true;
        this.angle = (360 - (Number((params && params.angle) || 0) + 30)) % 360;
        this._ledLogic();
    }

    _onMicStatus(disabled) {
        this.isMicDisabled = disabled;
        this._ledLogic();
    }

    _onAIPFocusChange(state) {
        if (state === FocusState.FOREGROUND) {
            if (this.angle) {
                this.isWakeup = true;
                this._ledLogic();
            }
        }
    }

    _onTTSFocusChange(state, by) {
        this.isTTSPlaying = state !== FocusState.NONE;
        if (this.isTTSPlaying || by !== ChannelType.AIP) {
            this._ledLogic();
        }
    }

    _onAlertFocusChange(state, by) {
        this.isAlertPlaying = state !== FocusState.NONE;
        if (this.isAlertPlaying || by !== ChannelType.AIP) {
            this._ledLogic();
        }
    }
}

module.exports.EVSModuleLed = EVSModuleLed;