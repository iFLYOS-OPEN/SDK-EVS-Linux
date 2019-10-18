var Log = require('console').Log;
var EventEmitter = require('events');
var EVSMediaPlayer = require('./evsMediaPlayer').EVSMediaPlayer;
var PlayerActivity = require('./evsMediaPlayer').PlayerActivity;
var FocusState = require('./evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('AudioHandler'));

var BGMode = {
    PAUSE: 'PAUSE',
    VOL_DOWN: 'VOL_DOWN'
};

/**
 * EVS General media player handler implementation.
 * Maintain a play list according to the state change of channel and player.
 * @class
 */
class EVSPlayerHandler extends EventEmitter {
    /**
     * Create an EVSPlayerHandler instance
     * @param {EVSAudioMgr} audioMgr
     * @param {string} channelName - channel name, reference to ChannelType
     * @param {string} playerName - player name
     * @param {string} bgMode - player behavior under background mode, reference to BGMode
     * @fires EVSPlayerHandler#focusstate
     * @fires EVSPlayerHandler#playerstate
     */
    constructor(audioMgr, channelName, playerName, bgMode) {
        super();

        this.audioMgr = audioMgr;
        this.channelName = channelName;
        this.playerName = playerName;
        this.bgMode = bgMode || BGMode.PAUSE;

        this.playItem = null;
        this.waitList = [];

        this.volume = 45;
        this.isMuted = false;

        this.isActivePause = false;
        this.playAfterStop = true;

        this.focusState = FocusState.NONE;
        this.playerState = PlayerActivity.IDLE;
        this.player = new EVSMediaPlayer('mediaplayer', this.playerName);
        this.player.setVolume(this.volume);

        this.audioMgr.on(this.channelName, this._onFocusState.bind(this));
        this.player.on('state', this._onPlayerState.bind(this));
    }


    /**
     * Get player name
     * @returns {string} media player name
     */
    getName() {
        return this.playerName;
    }

    /**
     * Pause media player
     */
    pause() {
        if (this.playerState === PlayerActivity.PLAYING) {
            this.isActivePause = true;
            this.player.pause();
        }
    }

    /**
     * Resume media player
     */
    resume() {
        if (this.playerState === PlayerActivity.PAUSED) {
            this.isActivePause = false;
            this._acquireChannel();
        }
    }

    /**
     * Stop media player
     * @param {boolean} playAfterStop
     */
    stop(playAfterStop) {
        if (this.playerState === PlayerActivity.PLAYING ||
            this.playerState === PlayerActivity.PAUSED) {
            this.playAfterStop = playAfterStop || false;
            this.player.stop();
        }
    }

    getVolume() {
        return this.volume;
    }

    /**
     * Set volume
     * @param {number} volume - the absolute volume value, range is 0 ~ 100
     */
    setVolume(volume) {
        this.volume = volume > 100 ? 100 : (volume < 0 ? 0 : volume);
        if (this.isMuted) {
            this.setMute(false);
        }
        this.player.setVolume(this.volume);
    }

    /**
     * Adjust volume
     * @param {number} delta - the volume change value, such as 10, 40, -30
     */
    adjustVolume(delta) {
        this.volume += delta;
        if (this.volume < 0) {
            this.volume = 0;
        } else if (this.volume > 100) {
            this.volume = 100;
        }
        if (this.isMuted) {
            this.setMute(false);
        }
        this.player.setVolume(this.volume);
    }

    /**
     * Set mute/unmute
     */
    setMute(mute) {
        this.isMuted = mute;
        this.player.setMute(mute);
    }

    _onFocusState(state) {
        if (this.focusState !== state) {
            this.focusState = state;

            if (this.focusState === FocusState.NONE) {
                if (!this.isActivePause) {
                    // 若非用户主动暂停造成的失去音频焦点，则停止当前播放器并清空播放列表
                    this.waitList.length = 0;
                    this.player.stop();
                }
            } else if (this.focusState === FocusState.FOREGROUND) {
                this._playForeground();
            } else if (this.focusState === FocusState.BACKGROUND) {
                this._playBackground();
            }

            /**
             * focus state change event.
             * @event EVSPlayerHandler#focusstate
             * @type {string} new focus state
             */
            this.emit('focusstate', this.focusState);
        }
    }

    _onPlayerState(state) {
        if (this.playerState !== state) {
            this.playerState = state;

            /**
             * player state change event.
             * @event EVSPlayerHandler#playerstate
             * @type {string} new player state
             */
            this.emit('playerstate', this.playerState);

            if (this.playerState === PlayerActivity.PAUSED) {
                if (this.isActivePause) {
                    // 若是主动暂停，则释放音频焦点
                    this._releaseChannel();
                }
            } else if (this.playerState === PlayerActivity.STOPPED ||
                this.playerState === PlayerActivity.FINISHED) {
                if (!this.playAfterStop || this.waitList.length === 0) {
                    // 若主动停止播放器或播放列表为空，则释放音频焦点
                    this._releaseChannel();
                } else {
                    // 若正常播放结束且播放列表不为空，则继续播放
                    this._playNext();
                }
            }
        }
    }

    _acquireChannel() {
        log(this.playerName, 'acquire channel ==>' + this.channelName);
        this.isActivePause = false;
        this.playAfterStop = true;
        this.audioMgr.acquireChannel(this.channelName);
    }

    _releaseChannel() {
        log(this.playerName, 'release channel ==>' + this.channelName);
        this.audioMgr.releaseChannel(this.channelName);
    }

    _playNext() {
        if (this.waitList.length > 0) {
            this.playItem = this.waitList.shift();
            var type = this.playItem.type || 'url';
            this.player.play(this.playItem.url, this.playItem.offset, type);

            if (this.bgMode !== BGMode.PAUSE) {
                if (this.focusState === FocusState.FOREGROUND) {
                    this.player.setVolume(this.volume);
                }
            }
        }
    }

    _playForeground() {
        if (this.bgMode === BGMode.PAUSE) {
            if (this.playerState === PlayerActivity.PAUSED) {
                this.player.resume();
            } else if (this.playerState !== PlayerActivity.PLAYING) {
                this._playNext();
            }
        } else {
            this.player.setVolume(this.volume);
            if (this.playerState !== PlayerActivity.PLAYING &&
                this.playerState !== PlayerActivity.PAUSED) {
                this._playNext();
            }
        }
    }

    _playBackground() {
        if (this.bgMode === BGMode.PAUSE) {
            if (this.playerState === PlayerActivity.PLAYING) {
                this.player.pause();
            }
        } else {
            this.player.setVolume(this.volume * 0.2);
            if (this.playerState !== PlayerActivity.PLAYING &&
                this.playerState !== PlayerActivity.PAUSED) {
                this._playNext();
            }
        }
    }
}

module.exports.EVSPlayerHandler = EVSPlayerHandler;
module.exports.BGMode = BGMode;
