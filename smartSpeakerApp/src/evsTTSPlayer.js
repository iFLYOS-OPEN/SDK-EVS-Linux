var Log = require('console').Log;
var EVSPlayerHandler = require('evsPlayerHandler').EVSPlayerHandler;
var PlayerActivity = require('evsMediaPlayer').PlayerActivity;
var ChannelType = require('evsAudioMgr').ChannelType;
var FocusState = require('evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('TTSPlayer'));

/**
 * @class
 * EVS tts player implementation
 */
class EVSTTSPlayer extends EVSPlayerHandler {
    /**
     * Create an EVSTTSPlayer instance
     * @param {EVSAudioMgr} audioMgr
     * @param {EVSClient} client
     */
    constructor(audioMgr, client) {
        super(audioMgr, ChannelType.TTS, 'tts_player');

        this.client = client;
        this.on('playerstate', this._onPlayerStateEx.bind(this));
    }

    /**
     * process tts directive
     * @param {object} item - tts directive object
     */
    onDirective(item) {
        // 若新的TTS的request_id为空或者与当前正在播放的TTS的request_id不同，则立即播放新的TTS
        if (!this.playItem || !this.playItem.request_id ||
            this.playItem.request_id !== item.request_id) {
            this.waitList.length = 0;
            this.stop(true);
        }
        this.waitList.push(item);

        // 若当前无音频焦点，则尝试获取音频焦点
        if (this.focusState === FocusState.NONE) {
            this._acquireChannel();
        }
    }

    _onPlayerStateEx(state) {
        if (state === PlayerActivity.PLAYING ||
            state === PlayerActivity.FINISHED) {
            var procSync = {
                header: {
                    name: 'audio_player.tts.progress_sync',
                    request_id: this.playItem.request_id
                },
                payload: {
                    resource_id: this.playItem.resource_id,
                    type: state === PlayerActivity.PLAYING ? 'STARTED' : 'FINISHED'
                }
            };
            this.client.request(procSync);
        }
    }
}

module.exports.EVSTTSPlayer = EVSTTSPlayer;
