var Log = require('console').Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('AudioManager'));

var ChannelType = {
    AIP: 'AIP',
    TTS: 'TTS',
    ALERT: 'ALERT',
    CONTENT: 'CONTENT',
    LOCAL: 'LOCAL',
};

var FocusState = {
    FOREGROUND: 'FOREGROUND',
    BACKGROUND: 'BACKGROUND',
    NONE: 'NONE'
};

/**
 * @class
 * EVS Audio channel manager. Be responsible for audio player channel focus.
 */
class EVSAudioMgr extends EventEmitter {
    /**
     * EVSAudioMgr constructor
     * @fires EVSAudioMgr#{ChannelType}
     */
    constructor() {
        super();

        this.channels = {};
        this.channels[ChannelType.AIP] = {
            priority: 30,
            state: FocusState.NONE,
            captureOn: [ChannelType.TTS, ChannelType.LOCAL, ChannelType.ALERT]
        };
        this.channels[ChannelType.TTS] = {
            priority: 10,
            state: FocusState.NONE,
            captureOn: []
        };
        this.channels[ChannelType.ALERT] = {
            priority: 40,
            state: FocusState.NONE,
            captureOn: [ChannelType.TTS, ChannelType.LOCAL, ChannelType.AIP]
        };
        this.channels[ChannelType.CONTENT] = {
            priority: 50,
            state: FocusState.NONE,
            captureOn: [ChannelType.ALERT, ChannelType.TTS, ChannelType.LOCAL, ChannelType.AIP]
        };
        this.channels[ChannelType.LOCAL] = {
            priority: 10,
            state: FocusState.NONE,
            captureOn: []
        };

        this.foregroundChannel = null;
        this.backgroundChannel = null;
    }

    /**
     * acquire channel
     * @param {string} name - channel name, reference to ChannelType
     */
    acquireChannel(name) {
        if (!this.channels[name]) {
            return;
        }

        if (this.foregroundChannel) {
            // 如果存在前景音频通道
            if (this.channels[name].captureOn.indexOf(this.foregroundChannel) !== -1) {
                // 如果当前前景音频通道在请求的音频通道的抢焦点列表中，
                // ---设置当前前景音频通道为无焦点状态
                // ---再重新请求音频通道焦点
                this._setChannelFocus(this.foregroundChannel, FocusState.NONE, name);
                this.acquireChannel(name);
            } else if (this.channels[name].priority === this.channels[this.foregroundChannel].priority) {
                // 如果当前前景音频通道的优先级与请求的音频通道的优先级相同
                // ---设置当前前景音频通道为无焦点状态
                // ---设置请求的音频通道为前景状态
                this._setChannelFocus(this.foregroundChannel, FocusState.NONE, name);
                this._setChannelFocus(name, FocusState.FOREGROUND);
            } else if (this.channels[name].priority < this.channels[this.foregroundChannel].priority) {
                // 如果当前前景音频通道的优先级低于请求的音频通道的优先级
                // ---如果当前存在背景音频通道，则先设置当前背景音频通道为无焦点状态
                // ---设置当前前景音频通道为背景状态
                // ---设置请求的音频通道为前景状态
                if (this.backgroundChannel) {
                    this._setChannelFocus(this.backgroundChannel, FocusState.NONE, name);
                }
                this._setChannelFocus(this.foregroundChannel, FocusState.BACKGROUND, name);
                this._setChannelFocus(name, FocusState.FOREGROUND);
            } else if (this.channels[name].priority > this.channels[this.foregroundChannel].priority) {
                // 如果当前前景音频通道的优先级高于请求的音频通道的优先级
                // ---如果当前不存在背景音频通道，则设置请求的音频通道为背景状态
                // ---如果当前存在背景音频通道且当前背景音频通道的优先级低于请求的音频通道的优先级
                //    ---设置当前的背景音频通道为无焦点
                //    ---设置请求的音频通道为背景状态
                if (!this.backgroundChannel) {
                    this._setChannelFocus(name, FocusState.BACKGROUND);
                } else if (this.channels[name].priority <= this.channels[this.backgroundChannel].priority) {
                    this._setChannelFocus(this.backgroundChannel, FocusState.NONE, name);
                    this._setChannelFocus(name, FocusState.BACKGROUND);
                }
            }
        } else if (this.backgroundChannel) {
            // 如果当前无前景音频通道，但有背景音频通道
            if (this.channels[name].priority < this.channels[this.backgroundChannel].priority) {
                // 如果当前背景音频通道的优先级低于请求的音频通道的优先级
                // ---设置请求的音频通道为前景状态
                this._setChannelFocus(name, FocusState.FOREGROUND);
            } else if (this.channels[name].priority === this.channels[this.backgroundChannel].priority) {
                // 如果当前背景音频通道的优先与请求的音频通道的优先级相同
                // ---设置当前的背景音频通道为无焦点
                // ---设置请求的音频通道为前景状态
                this._setChannelFocus(this.backgroundChannel, FocusState.NONE, name);
                this._setChannelFocus(name, FocusState.FOREGROUND);
            } else {
                // 如果当前背景音频通道的优先级高于请求的音频通道的优先级
                // ---设置当前的背景音频通道为前景状态
                // ---设置请求的音频通道为背景状态
                this._setChannelFocus(this.backgroundChannel, FocusState.FOREGROUND, name);
                this._setChannelFocus(name, FocusState.BACKGROUND);
            }
        } else {
            // 如果当前无前景音频通道也无背景音频通道
            // ---设置请求的音频通道为前景状态
            this._setChannelFocus(name, FocusState.FOREGROUND);
        }
    }

    /**
     * release channel
     * @param {string} name - channel name, reference to ChannelType
     */
    releaseChannel(name) {
        log('releaseChannel(', name, ') fore:', this.foregroundChannel, "back", this.backgroundChannel);
        if (!this.channels[name]) {
            return;
        }

        // 设置释放的音频通道为无焦点状态
        this._setChannelFocus(name, FocusState.NONE);
        if (!this.foregroundChannel && this.backgroundChannel) {
            // 设置当前背景音频通道为前景状态
            this._setChannelFocus(this.backgroundChannel, FocusState.FOREGROUND, name);
        }
    }

    _setChannelFocus(name, state, byWhich) {
        if (this.channels[name].state !== state) {
            if (this.foregroundChannel === name && state !== FocusState.FOREGROUND) {
                this.foregroundChannel = null;
            } else if (this.backgroundChannel === name && state !== FocusState.BACKGROUND) {
                this.backgroundChannel = null;
            }

            if (state === FocusState.FOREGROUND) {
                this.foregroundChannel = name;
            } else if (state === FocusState.BACKGROUND) {
                this.backgroundChannel = name;
            }

            this.channels[name].state = state;
            log('Channel.' + name, '==>', state);
            /**
             * audio channel focus changed event.
             * @event EVSAudioMgr#{ChannelType}
             * @type {string} new focus state, reference to FocusState
             */
            this.emit(name, state, byWhich);
        }
    }
}

module.exports.EVSAudioMgr = EVSAudioMgr;
module.exports.ChannelType = ChannelType;
module.exports.FocusState = FocusState;
