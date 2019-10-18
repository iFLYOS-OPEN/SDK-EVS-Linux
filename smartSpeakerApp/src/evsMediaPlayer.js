var Log = require('console').Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('MediaPlayer'));

var PlayerActivity = {
    IDLE: 'IDLE',
    PLAYING: 'PLAYING',
    PAUSED: 'PAUSED',
    STOPPED: 'STOPPED',
    FINISHED: 'FINISHED',
};

var ResourceType = {
    URL: 'url',
    MP3: 'mp3',
    PCM: 'pcm'
};

var router;

/**
 * EVS media player implementation. Interact with external media player and report the state change of internal player.
 * @class
 */
class EVSMediaPlayer extends EventEmitter {
    /**
     * Create an EVSMediaPlayer instance
     * @param {string} moduleName - module name
     * @param {string} playerName - player name
     * @fires EVSMediaPlayer#state
     * @fires EVSMediaPlayer#error
     */
    constructor(moduleName, playerName) {
        super();

        this.moduleName = moduleName || 'mediaplayer';
        this.playerName = playerName;

        this.activity = PlayerActivity.IDLE;
        this.volume = 45;
        this.offset = 0;
        this.playing = null;
        //this.isPreparing = false;

        router = require('iflyos').router;
        router.on('DO_MEDIAPLAYER_REPORT_CONTROL_RESULT', this._onControlResult.bind(this));
        router.on('DO_MEDIAPLAYER_REPORT_SOURCE_ID', this._onSourceId.bind(this));
        router.on('DO_MEDIAPLAYER_REPORT_VOLUME', this._onVolume.bind(this));
        router.on('DO_MEDIAPLAYER_REPORT_PLAY_STATUS', this._onPlayStatus.bind(this));
        this._playerControl('EVT_MEDIAPLAYER_CREATE');
    }

    /**
     * Start to play an audio resource
     * @param {string} path - resource path which can be url or local path
     * @param {string} offset - playback offset
     * @param {string} type - resource type, reference to ResourceType
     */
    play(path, offset, type) {
        if (this.playing) {
            /**
             * player error event.
             * @event EVSMediaPlayer#error
             * @type {Error} error information
             */
            this.emit('error', new Error('player_busy'));
        } else {
            this.playing = {path: path, type: type};
            var params = [this.playerName, type, path, offset ? offset.toString() : 'false'];
            router.send(this.moduleName, 'EVT_MEDIAPLAYER_SET_SOURCE', params);
            this._changeActivity(PlayerActivity.PLAYING);
            //this.isPreparing = true;
        }
    }

    /**
     * Stop internal player
     */
    stop() {
        // if (this.activity === PlayerActivity.PLAYING ||
        //     this.activity === PlayerActivity.PAUSED) {
        //     this._playControl('EVT_MEDIAPLAYER_STOP');
        // } else {
        if (this.playing) {
            if (this.playing.resourceId) {
                this._playControl('EVT_MEDIAPLAYER_STOP');
            } else {
                this.playing.cancel = true;
            }
        }
    }

    /**
     * Pause internal player
     */
    pause() {
        if (this.activity === PlayerActivity.PLAYING) {
            this._playControl('EVT_MEDIAPLAYER_PAUSE');
        }
    }

    /**
     * Resume internal player
     */
    resume() {
        if (this.activity === PlayerActivity.PAUSED) {
            this._playControl('EVT_MEDIAPLAYER_RESUME');
        }
    }

    /**
     * Get current volume
     * @returns {number} current volume
     */
    getVolume() {
        return this.volume;
    }

    /**
     * Set volume
     * @param {number} volume - the absolute volume value, range is 0 ~ 100
     */
    setVolume(volume) {
        if (volume >= 0 && volume <= 100) {
            router.send(this.moduleName, 'EVT_MEDIAPLAYER_SET_VOLUME', [this.playerName, volume.toString()]);
        }
    }

    /**
     * Adjust volume
     * @param {number} delta - the volume change value, such as 10, 40, -30
     */
    adjustVolume(delta) {
        var change;
        if (delta < 0 && delta >= -100) {
            change = delta.toString();
        } else if (delta > 0 && delta <= 100) {
            change = '+' + delta.toString();
        } else {
            return;
        }

        router.send(this.moduleName, 'EVT_MEDIAPLAYER_SET_VOLUME', [this.playerName, change]);
    }

    /**
     * Set mute/unmute
     */
    setMute(mute) {
        router.send(this.moduleName, 'EVT_MEDIAPLAYER_SET_MUTE', [this.playerName, mute.toString()]);
    }

    /**
     * Get player offset
     * @param {function} callback - return the offset
     */
    getOffset(callback) {
        if (this.activity === PlayerActivity.PLAYING ||
            this.activity === PlayerActivity.PAUSED) {
            if (this.playing.resourceId) {
                router.once('DO_MEDIAPLAYER_REPORT_OFFSET', (name, id, offset) => callback(Number(offset)));
                this._playControl('EVT_MEDIAPLAYER_REPORT_OFFSET');
            } else {
                callback(0);
            }
        } else {
            callback(-1);
        }
    }

    _onControlResult(name, id, cmd, result) {
        if (name === this.playerName) {
        }
    }

    _onSourceId(name, id) {
        if (name === this.playerName) {
            if (!id) {
                this.playing = null;
                this.emit('error', new Error('error_source_id'));
                this._changeActivity(PlayerActivity.STOPPED);
            } else {//if (this.playing) {
                this.playing.resourceId = id;
                if (!!this.playing.cancel) {
                    this._playControl('EVT_MEDIAPLAYER_STOP');
                    this.playing = null;
                    this._changeActivity(PlayerActivity.STOPPED);
                } else {
                    this._playControl('EVT_MEDIAPLAYER_PLAY');
                }
            }
        }
    }

    _onVolume(name, volume) {
        if (name === this.playerName) {
            this.volume = volume;
        }
    }

    _onPlayStatus(name, id, state) {
        if (name === this.playerName) {
            //this.isPreparing = false;
            if (state === 'play' || state === 'resume') {
                this._changeActivity(PlayerActivity.PLAYING);
            } else if (state === 'pause') {
                this._changeActivity(PlayerActivity.PAUSED);
            } else if (state === 'finish') {
                this.playing = null;
                this._changeActivity(PlayerActivity.FINISHED);
            } else if (state === 'stop') {
                // 防止部分播放器在finish之后又发送了stop
                if (this.activity !== PlayerActivity.FINISHED) {
                    this.playing = null;
                    this._changeActivity(PlayerActivity.STOPPED);
                }
            } else if (state === 'error') {
                this.playing = null;
                this._changeActivity(PlayerActivity.STOPPED);
                this.emit('error', new Error('internal_player_error'));
            }
        }
    }

    _playerControl(cmd) {
        router.send(this.moduleName, cmd, this.playerName);
    }

    _playControl(cmd) {
        if (this.playing && this.playing.resourceId) {
            router.send(this.moduleName, cmd, [this.playerName, this.playing.resourceId]);
        }
    }

    _changeActivity(activity) {
        if (this.activity !== activity) {
            this.activity = activity;
            log(this.playerName, '==>', this.activity);
            /**
             * player state changed event.
             * @event EVSMediaPlayer#state
             * @type {string} current player state, reference to PlayerActivity
             */
            this.emit('state', this.activity);
        }
    }
}

module.exports.EVSMediaPlayer = EVSMediaPlayer;
module.exports.PlayerActivity = PlayerActivity;
module.exports.ResourceType = ResourceType;
