var Log = require('console').Log;
var EventEmitter = require('events');
var FocusState = require('evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('CapAudioPlayer'));

/**
 * @class
 * EVS Audio Player capability implementation
 */
class EVSCapAudioPlayer extends EventEmitter {
    /**
     * Create a EVSCapAudioPlayer instance
     * @param {EVSClient} client
     * @param {EVSContextMgr} cxtMgr
     * @param {EVSAudioMgr} audioMgr
     * @param {EVSCapSystem} capSystem
     */
    constructor(client, recognizer, ttsPlayer, alertPlayer, audioPlayer) {
        super();

        this.ttsPlayer = ttsPlayer;
        this.alertPlayer = alertPlayer;
        this.audioPlayer = audioPlayer;

        this.curDialogId = null;
        this.playbackList = [];
        this.expectItem = null;
        this.recognizer = recognizer;

        client.onDirective('audio_player.audio_out', this._onAudioOut.bind(this));
        client.onDirective('recognizer.expect_reply', this._onExpectReply.bind(this));
        client.on('disconnected', this._onDisconnected.bind(this));
        this.ttsPlayer.on('focusstate', this._onTTSFocusState.bind(this));
    }

    _onAudioOut(response) {
        var payload = response.payload;
        payload.request_id = response.header.request_id;
        payload.type = payload.type.toUpperCase();

        log('On audio_out:', payload);

        if (payload.type === 'TTS') {
            if (payload.behavior !== "PARALLEL") {
                this.curDialogId = payload.request_id;
            }
            this.ttsPlayer.onDirective(payload);
        } else if (payload.type === 'RING') {
            this.alertPlayer.onDirective(payload);
        } else if (payload.type === 'PLAYBACK') {
            if (payload.request_id && payload.request_id === this.curDialogId) {
                this.playbackList.push(payload);
            } else {
                this.audioPlayer.onDirective(payload);
            }
        }
    }

    _onExpectReply(response) {
        log('On expect_reply:', response);
        if (response.header.request_id === this.curDialogId) {
            this.expectItem = response;
        } else {
            this.recognizer.expectReply(response);
        }
    }

    _onTTSFocusState(state) {
        if (state === FocusState.NONE) {
            this.playbackList.forEach((cur, index, array) => this.audioPlayer.onDirective(cur));
            this.playbackList.length = 0;

            if (this.expectItem) {
                this.recognizer.expectReply(this.expectItem);
                this.expectItem = null;
            }
        }
    }

    _onDisconnected() {
        this.playbackList.length = 0;
        this.audioPlayer.stop();
    }
}

module.exports.EVSCapAudioPlayer = EVSCapAudioPlayer;
