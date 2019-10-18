var fs = require("fs");
var console = require('console');

class PcmPlayer {
    constructor(channels, sampleFormat, sampleRate) {
        native.create(this, channels, sampleFormat, sampleRate);

        this.volume = 100;
    }

    playFile(file, callback = null) {
        if (!file) {
            callback && callback(new Error('argument_error'));
        } else {
            fs.readFile(file, (err, buf) => {
                if (err) {
                    callback && callback(err);
                } else {
                    native.play(this, buf, callback);
                }
            });
        }
    }

    playStream(buffer, callback = null) {
        if (!buffer) {
            callback && callback(new Error('argument_error'));
        } else {
            native.play(this, buffer, callback);
        }
    }

    stop() {
        native.stop(this);
    }

    volumeDown() {
        this.volume = (this.volume - 10) < 0 ? 0 : (this.volume - 10);
        native.volumeDown(this);
    }

    volumeUp() {
        this.volume = (this.volume + 10) > 100 ? 100 : (this.volume + 10);
        native.volumeUp(this);
    }

    setVolume(volume) {
        console.log("pcm_player, new vol:", volume / 100);
        this.volume = volume;
        native.setVolume(this, volume / 100);
    }

    setMute(mute) {
        if (mute) {
            this.setVolume(0);
        } else {
            this.setVolume(this.volume);
        }
    }

    adjuestVolume(delta) {
        if (delta < 0) {
            this.volumeDown();
        } else {
            this.volumeUp();
        }
    }
}

module.exports = PcmPlayer;
module.exports.PcmPlayer = PcmPlayer;

module.exports.paInt8 = 0x00000010;
module.exports.paInt16 = 0x00000008;
module.exports.paInt24 = 0x00000004;
module.exports.paInt32 = 0x00000002;
