var Log = require('console').Log;
var EVSPlayerHandler = require('evsPlayerHandler').EVSPlayerHandler;
var BGMode = require('evsPlayerHandler').BGMode;
var PlayerActivity = require('evsMediaPlayer').PlayerActivity;
var ChannelType = require('evsAudioMgr').ChannelType;
var FocusState = require('evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('AudioPlayer'));

/**
 * @class
 * EVS playback player implementation
 */
class EVSAudioPlayer extends EVSPlayerHandler {
    /**
     * Create an EVSAudioPlayer instance
     * @param {EVSAudioMgr} audioMgr
     * @param {EVSClient} client
     * @param {EVSContextMgr} cxtMgr
     */
    constructor(audioMgr, client, cxtMgr) {
        super(audioMgr, ChannelType.CONTENT, 'content_player', BGMode.VOL_DOWN);

        this.client = client;
        this.delayedPlay = null;

        cxtMgr.addContextProvider(this._getContext.bind(this));
        this.on('playerstate', this._onPlayerStateEx.bind(this));
        this.player.on('error', this._onPlayerError.bind(this));
    }

    /**
     * process playback directive
     * @param {object} item - playback directive object
     */
    onDirective(item) {
        item.control = item.control.toUpperCase();
        item.behavior = item.behavior && item.behavior.toUpperCase();

        if (item.control === 'PLAY') {
            // 如果behavior为IMMEDIATELY，则清空当前播放列表，并停止当前的播放
            // stop函数的true参数，让播放器在进入STOPPED状态后能继续播放
            this.waitList.length = 0;
            if (item.behavior === 'IMMEDIATELY') {
                this.stop(true);
            }

            // 有duration字段，且值为null，则为直播
            if (item.hasOwnProperty('duration') && !item.duration) {
                item.offset = 0;
                log('live playback, ignore offset');
            }
            this.waitList.push(item);

            // 若当前无音频焦点，则尝试获取音频焦点
            if (this.focusState === FocusState.NONE) {
                this._acquireChannel();
            }
        } else if (item.control === 'PAUSE') {
            // EVS下发的PAUSE指令，设备端以stop方式来处理，同时需上报播放器的状态，详细见_onPlayerStateEx
            // 在pause或stop该播放器时，需要先获取播放器的offset，再执行pause或者stop操作
            // 继续播放时，服务端会下发带offset参数的PLAY指令
            // stop函数的false参数，让播放器在进入STOPPED状态后不能继续播放
            this.stop(false);
        } else if (item.control === 'RESUME') {
            this.resume();
        } else if (item.control === 'STOP') {
            this.stop(false);
        }
    }

    /**
     * Pause media player
     */
    pause() {
        this._cancelDelayedPlay();
        if (this.playerState === PlayerActivity.PLAYING) {
            this.isPauseCalled = true;
            // 暂停之前先获取offset
            this.player.getOffset((offset) => {
                this.playItem.offset = offset;
                this.player.pause();
            });
        }
    }

    /**
     * Stop media player
     * @param {boolean} playAfterStop
     */
    stop(playAfterStop) {
        this._cancelDelayedPlay();
        if (this.playerState === PlayerActivity.PLAYING ||
            this.playerState === PlayerActivity.PAUSED) {
            this.playAfterStop = playAfterStop || false;
            // 停止之前先获取offset
            this.player.getOffset((offset) => {
                this.playItem.offset = offset;
                this.player.stop();
            });
        }
    }

    _cancelDelayedPlay() {
        if (this.delayedPlay) {
            clearTimeout(this.delayedPlay);
            this.delayedPlay = undefined;
        }
    }

    _play() {
        this._cancelDelayedPlay();
        this._playForeground();
    }

    /**
     * Get player context
     * @returns {object/Promise} context object / a promise with context object resolve
     */
    _getContext() {
        var context = {
            audio_player: {
                version: "1.1",
                playback: {}
            },
        };

        if (this.playerState === PlayerActivity.IDLE) {
            // 当播放器状态为IDLE时，无需source id和offset参数
            context.audio_player.playback.state = 'IDLE';
            return context;
        } else if (this.playerState === PlayerActivity.PAUSED ||
            this.playerState === PlayerActivity.STOPPED ||
            this.playerState === PlayerActivity.FINISHED) {
            // 当播放器为PAUSED, STOPPED和FINISHED状态时，统一上报PAUSED状态
            context.audio_player.playback.state = 'PAUSED';
            context.audio_player.playback.resource_id = this.playItem.resource_id;
            if (this.playItem && this.playItem.offset) {
                context.audio_player.playback.offset = this.playItem.offset;
            } else {
                context.audio_player.playback.offset = 0;
            }
            return context;
        } else if (this.playerState === PlayerActivity.PLAYING) {
            // 当播放器为PLAYING状态时，由于没有当前的offset信息，而且获取offset是异步操作，所以返回promise
            context.audio_player.playback.state = 'PLAYING';
            context.audio_player.playback.resource_id = this.playItem.resource_id;
            return new Promise((resolve, reject) => {
                this.player.getOffset((offset) => {
                    context.audio_player.playback.offset = offset;
                    resolve(context);
                });
            });
        }
    }

    _onFocusState(state, byWhich) {
        if (this.focusState !== state) {
            this.focusState = state;
            this.emit('focusstate', this.focusState);

            if (this.isPauseCalled || !this.playAfterStop) {
                this._releaseChannel();
            } else if (this.focusState === FocusState.NONE) {
                // 若非用户主动暂停造成的失去音频焦点，则停止当前播放器并清空播放列表
                this.waitList.length = 0;
                this.player.stop();
            } else if (this.focusState === FocusState.FOREGROUND) {
                log(this.channelName, 'become foreground because', byWhich);
                if (byWhich === ChannelType.AIP) {
                    log('delay resume because AIP release channel and new content control command may coming...');
                    this._cancelDelayedPlay();
                    this.delayedPlay = setTimeout(this._play.bind(this), 1500);
                } else {
                    this._play();
                }
            } else if (this.focusState === FocusState.BACKGROUND) {
                this._cancelDelayedPlay();
                this._playBackground();
            }
        }
    }

    _onPlayerStateEx(state) {
        if (state === PlayerActivity.PLAYING ||
            state === PlayerActivity.STOPPED ||
            state === PlayerActivity.FINISHED) {
            var procSync = {
                header: {
                    name: 'audio_player.playback.progress_sync'
                },
                payload: {
                    resource_id: this.playItem.resource_id
                }
            };

            if (state === PlayerActivity.PLAYING) {
                procSync.payload.type = 'STARTED';
                this.client.request(procSync);
            } else if (state === PlayerActivity.STOPPED) {
                procSync.payload.type = 'PAUSED';
                if (this.playItem && this.playItem.offset) {
                    procSync.payload.offset = this.playItem.offset;
                } else {
                    procSync.payload.offset = 0;
                }
                this.client.request(procSync);
            } else if (state === PlayerActivity.FINISHED) {
                procSync.payload.type = 'NEARLY_FINISHED';
                procSync.payload.offset = this.playItem.offset;
                this.client.request(procSync).then(()=>{
                    procSync.payload.type = 'FINISHED';
                    this.client.request(procSync);
                });
            }
        }
    }

    _onPlayerError(err) {
        var procSync = {
            header: {
                name: 'audio_player.playback.progress_sync'
            },
            payload: {
                type: 'FAILED',
                resource_id: this.playItem.resource_id
            }
        };

        if (err.message === 'player_busy') {
            procSync.payload.failure_code = 1002;
        } else if (err.message === 'error_source_id') {
            procSync.payload.failure_code = 1002;
        } else if (err.message === 'internal_player_error') {
            procSync.payload.failure_code = 1002;
        } else {
            procSync.payload.failure_code = 1001;
        }
        this.client.request(procSync);
    }
}

module.exports.EVSAudioPlayer = EVSAudioPlayer;
