var fs = require('fs');
var Log = require('console').Log;
var EventEmitter = require('events');

var EVSPlayerHandler = require('./evsPlayerHandler').EVSPlayerHandler;
var BGMode = require('./evsPlayerHandler').BGMode;
var PlayerActivity = require('./evsMediaPlayer').PlayerActivity;
var ChannelType = require('./evsAudioMgr').ChannelType;
var FocusState = require('./evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('SoundPlayer'));

/**
 * @class
 * EVS local sound effect player handler implementation
 */
class EVSSoundPlayer extends EVSPlayerHandler {
    /**
     * Create a EVSContentHandler instance
     * @param {EVSAudioMgr} audioMgr
     * @param {string} dirPath
     */
    constructor(audioMgr, dirPath) {
        super(audioMgr, ChannelType.LOCAL, 'sound_player', BGMode.PAUSE);

        this.path = dirPath;
        if(this.path[this.path.length - 1] !== '/'){
            this.path += '/';
        }
        this.files = fs.readdirSync(this.path);
        this.files.sort();

        this.on('playerstate', this._onPlayerStateEx.bind(this));
    }

    playFile(filePath, callback){
        this.waitList.length = 0;
        this.waitList.push({url: filePath, type: 'mp3', cb: callback});
        this.stop(true);

        if (this.focusState === FocusState.NONE) {
            this._acquireChannel();
        }
    }

    /**
     * play local audio file
     * @param {string} file - the name of local audio file
     * @param {function} callback -
     */
    play(name, callback) {
        var filePath = this._getFile(name);
        if (!filePath) {
            callback && callback(new Error('bad_argument'));
            return;
        }

        this.playFile(filePath, callback);
    }

    _getFile(name) {
        var ret = this.files.find(f => {
            return name === f.substr(0, name.length);
        });

        if (!ret) {
            console.error('can not find sound effect:', name);
            return '';
        }
        return this.path + ret;
    }

    _onPlayerStateEx(state) {
        if (state === PlayerActivity.FINISHED ||
            state === PlayerActivity.STOPPED) {
            if (this.playItem.cb) {
                this.playItem.cb();
            }
        }
    }
}

module.exports.EVSSoundPlayer = EVSSoundPlayer;
