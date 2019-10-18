var console = require('console');
var fs = require('fs');

var EVSMediaPlayer = require('../../evsMediaPlayer').EVSMediaPlayer;
var PlayerActivity = require('../../evsMediaPlayer').PlayerActivity;

class EVSEffectPlayer {
    constructor(dirPath) {
        this.path = dirPath;
        if (this.path[this.path.length - 1] !== '/') {
            this.path += '/';
        }

        this.playItem = null;
        this.nextItem = null;

        this.volume = 45;
        this.playerState = PlayerActivity.IDLE;

        this.player = new EVSMediaPlayer('mediaplayer', 'effect_player');
        this.player.setVolume(this.volume);
        this.player.on('state', this._onPlayerState.bind(this));
    }

    play(file, callback) {
        this.nextItem = {file: this.path + file + '.mp3', 'cb': callback};
        if (this.playerState === PlayerActivity.PLAYING ||
            this.playerState === PlayerActivity.PAUSED) {
            this.player.stop();
        } else {
            this._playNext();
        }
    }

    stop() {
        this.player.stop();
    }

    getName() {
        return 'effect_player';
    }

    setVolume(vol) {
        this.volume = vol;
        this.player.setVolume(this.volume);
    }

    adjustVolume(delta) {
        this.volume += delta;
        if (this.volume < 0) {
            this.volume = 0;
        } else if (this.volume > 100) {
            this.volume = 100;
        }
        this.player.setVolume(this.volume);
    }

    setMute() {
        this.player.setMute();
    }

    _onPlayerState(state) {
        this.playerState = state;
        if (state === PlayerActivity.FINISHED ||
            state === PlayerActivity.STOPPED) {
            if (this.playItem.cb) {
                this.playItem.cb();
            }
            this._playNext();
        }
    }

    _playNext() {
        if (this.nextItem) {
            this.playItem = this.nextItem;
            this.nextItem = null;
            this.player.play(this.playItem.file, 0, 'mp3');
        }
    }
}

module.exports.EVSEffectPlayer = EVSEffectPlayer;
