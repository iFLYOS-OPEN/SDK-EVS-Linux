var fs = require('fs');
var Log = require('console').Log;
var EventEmitter = require('events');
var ChannelType = require('evsAudioMgr').ChannelType;
var FocusState = require('evsAudioMgr').FocusState;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('CapRecog'));

/**
 * @class EVSCapRecognizer
 * recognizer client implementation
 */
class EVSCapRecognizer extends EventEmitter{
    /**
     *
     * @param {EVSClient}client
     * @param {EVSContextMgr}cxtMgr
     * @param {EVSCapSystem} capSystem
     * @param {EVSAudioMgr} audioMgr
     * @param {iflyos.Voice} voice
     */
    constructor(client, cxtMgr, capSystem, audioMgr, voice){
        super();
        this.cxtMgr = cxtMgr;
        this.state = 'idle';
        this.client = client;
        this.audioMgr = audioMgr;
        this.currentReqId = '';
        this.voice = voice;
        this.hasAudioFocus = false;
        this.voiceStreamTerminated = true;
        this.onVoiceCB = this._onVoice.bind(this);
        this.voice.on('voice', this.onVoiceCB);
        this.thinkingTimeout = undefined;
        cxtMgr.addContextProvider(this._getContext.bind(this));
        client.on('response_proc_end', this._onRspEnd.bind(this));
        client.on('disconnected', this._onDisconnected.bind(this));
        client.onDirective('recognizer.intermediate_text', this._onASRText.bind(this));
        //client.onDirective('recognizer.expect_reply', this._onExpect.bind(this));
        client.onDirective('recognizer.stop_capture', ()=>{
            client.endVoice();
            this.voiceStreamTerminated = true;
            this.voice.enableStreamPuller(false);
            if(this.hasAudioFocus){
                this.audioMgr.releaseChannel(ChannelType.AIP);
            }
            this.state = 'thinking';
            this.emit('thinking');
            this.thinkingTimeout = setTimeout(this._onThinkingTimeout.bind(this), 5000);
        });

        this.audioMgr.on(ChannelType.AIP, this._onFocusChanged.bind(this));
    }

    /**
     * NLP process text
     * @param text
     * @param reply_key
     */
    nlp(text, tts, reply_key){
        if(this.state != 'idle'){
            if(this.state == 'record'){
                this.client.cancelVoice();
                this.voice.enableStreamPuller(false);
            }
            if(this.thinkingTimeout){
                clearTimeout(this.thinkingTimeout);
                this.thinkingTimeout = undefined;
            }
        }

        this.currentReqId = EVSCapRecognizer._uuid();
        var req = {
            header: {
                name: "recognizer.text_in",
                request_id: this.currentReqId
            },
            payload: {
                query: text,
                with_tts: !!tts,
                reply_key: reply_key || ''
            }
        };
        /**
         * @event EVSCapRecognizer#thinking server is thinking
         */
        this.state = 'thinking';
        this.emit('thinking');
        this.thinkingTimeout = setTimeout(this._onThinkingTimeout.bind(this), 5000);
        this.client.setCurrentRequestId(this.currentReqId);
        this.client.request(req);
    }

    /**
     * start recognize
     * @param {string}reply_key reply_key from expect
     * @param {boolean}needVoiceChannel recognize need stop tts?
     * @fires EVSCapRecognizer#listening
     * @fires EVSCapRecognizer#thinking
     * @fires EVSCapRecognizer#idle
     */
    recognize(reply_key, needVoiceChannel){
        //true if param missing
        needVoiceChannel = (needVoiceChannel == undefined) || !!needVoiceChannel;
        log('recognize(',reply_key, needVoiceChannel,')');
        if(this.state != 'idle'){
            if(this.state == 'record'){
                this.client.cancelVoice();
                this.voice.enableStreamPuller(false);
            }
            if(this.thinkingTimeout){
                clearTimeout(this.thinkingTimeout);
                this.thinkingTimeout = undefined;
            }
            this.state = 'idle';
        }

        this.currentReqId = EVSCapRecognizer._uuid();
        this.request = {
            header: {
                name: "recognizer.audio_in",
                request_id: this.currentReqId,
            },
            payload: {
                reply_key: reply_key || '',
                enable_vad: true,
                profile: "FAR_FIELD",
                format: "AUDIO_L16_RATE_16000_CHANNELS_1"
                // "iflyos_wake_up": {
                //     "score": 666,
                //     "start_index_in_samples": 50,
                //     "end_index_in_samples": 150,
                //     "word": "蓝小飞",
                //     "prompt": "我在"
                // },
            }
        };

        this.cxtMgr.getContext().then(c=>{
            this.latestContext = c;
            if(needVoiceChannel){
                this.audioMgr.acquireChannel(ChannelType.AIP);
            }else{
                this._onFocusChanged(FocusState.FOREGROUND);
            }

        });

    }

    _onFocusChanged(state) {
        if (state === FocusState.FOREGROUND) {
            this.hasAudioFocus = true;
            this.client.setCurrentRequestId(this.currentReqId);
            this.state = 'record';
            this.voice.enableStreamPuller(true);
            this.client.request(this.request, this.latestContext);
            /**
             * @event EVSCapRecognizer#listening server is listening device
             */
            this.emit('listening');
        } else if (state === FocusState.NONE) {
            this.hasAudioFocus = false;
            this._stopRecord();
        }
    }

    _onVoice(voice){
        if(this.state == 'record') {
            this.client.writeVoice(voice);
        }
    }

    _cancelThinking(){
        if(this.state == 'thinking'){
            clearTimeout(this.thinkingTimeout);
            this.thinkingTimeout = undefined;
        }
    }

    _stopRecord(){
        if (this.state == 'record') {
            (!this.voiceStreamTerminated) && this.client.cancelVoice();
            this.voice.enableStreamPuller(false);
        }
        if(this.hasAudioFocus){
            this.audioMgr.releaseChannel(ChannelType.AIP);
        }
        this.voiceStreamTerminated = true;
    }

    _terminalDialog(){
        if(this.state != 'idle'){
            this._stopRecord();
            this._cancelThinking();
            this.state = 'idle';
            /**
             * @event EVSCapRecognizer#idle no activity
             */
            this.emit('idle');
        }
    }

    _onASRText(rsp){
        log('asr:', rsp.payload.text);
    }

    expectReply(rsp){
        if(this.currentReqId != rsp.header.request_id){
            log('drop expect because request id changed', this.currentReqId, rsp.header.request_id);
            return;
        }
        this.recognize(rsp.payload.reply_key, rsp.payload.background_recognize != true);
    }

    _onRspEnd(reqId){
        log('responses for request', reqId, 'ended');
        if(reqId == this.currentReqId){
            this._terminalDialog();
        }
    }

    _onThinkingTimeout(){
        log('thinking timeout!');
        this._terminalDialog();
    }

    _onDisconnected(){
        this.client.setCurrentRequestId(EVSCapRecognizer._uuid());
        this._terminalDialog();
    }

    _getContext(){
        return{
            recognizer: {
                version: "1.1"
            }
        };
    }

    static _uuid(){
        var buf = fs.readFileSync('/proc/sys/kernel/random/uuid');
        return "manual-" + buf.toString(-1, 0, buf.length - 1);
    }
}

exports.EVSCapRecognizer = EVSCapRecognizer;