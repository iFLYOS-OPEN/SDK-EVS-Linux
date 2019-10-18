var fs = require('fs');
var EventEmitter = require('events');
var EasyCurl = require('curl');
var WebSocket = require('websocket').Websocket;
var Log = require('console').Log;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('EVSClient'));


/**
 * @class EVSClient
 * EVS client, manage the websocket connection, route responses.
 */
class EVSClient extends EventEmitter{
    /**
     * @param {string}serviceUrl websocket url, like 'wss://ivs.iflyos.cn/embedded/v1beta2'
     * @param {string}token
     * @param {string}deviceId
     * @param {EVSContextMgr} cxtMgr
     */
    constructor(serviceUrl, deviceId, cxtMgr){
        super();
        // this.token = token;
        // this.deviceId = deviceId;
        // {
        //     deviceId = EasyCurl.escape(deviceId);
        //     this.serviceUrl = serviceUrl + "?token=" + token + "&device_id=" + deviceId;
        // }
        this.wssUrl = serviceUrl;
        this.token = undefined;
        this.deviceId = deviceId;
        this.escapedId = EasyCurl.escape(deviceId);
        this.serviceUrl = undefined;

        this.initPromises = [];
        this.cxtMgr = cxtMgr;
        this.directiveEmitter = new EventEmitter();
        this.connected = false;
        this.running = false;
        this._onWsOpenFn = this._onWsOpen.bind(this);
        this._onWsCloseFn = this._onWsClose.bind(this);
        this._onWsErrorFn = this._onWsError.bind(this);
        this._onWsMessageFn = this._onWsMessage.bind(this);
        this.recogReqId = '';//'' for ignore reqId
        // this.deviceInfo = {
        //     authorization: "Bearer " + this.token,
        //     device:{
        //          device_id: this.deviceId,
        //     //     "ip": "xxx.xxx.xxx.xxx",
        //     //     "location": {
        //     //         "latitude": 132.56481,
        //     //         "longitude": 22.36549
        //     //     },
        //          platform: {
        //              name: "linux",
        //              version: "ubuntu 19.04"
        //          }
        //     }
        // };
        this.deviceInfo = undefined;
    }

    /**
     *@param promise a promise should be resolved before EVSClient#ready
     */
    add2InitList(promise){
        this.initPromises.push(promise);
    }

    /**
     * startServerConnection local initializations
     * @fires EVSClient#ready
     */
    startLocalInit(){
        log('startLocalInit');
        /**
         * local init done, ready for local operations, like playing audio, record, set alarm, collect context
         * @event EVSClient#ready
         */
        if(this.initPromises.length > 0){
            Promise.all(this.initPromises).then(()=>{
                this.emit('ready');
            }).catch(err=>{
                log("init error:", err);
            });
            this.initPromises = [];
        } else {
            this.emit('ready');
        }
    }

    /**
     * start ws connection
     * @fires EVSClient#connected
     * @fires EVSClient#disconnected
     * @fires EVSClient#response_recv_end
     * @fires EVSClient#response_proc_end
     */
    startServerConnection(){
        log('startServerConnection()');
        this.running = true;
        if(this.connected) return;
        /**
         * ws connected event
         * @event EVSClient#connected
         */

        /**
         * ws disconnected event
         * @event EVSClient#disconnected
         */

        //wss://ivs.iflyos.cn:3443/embedded/v1beta2
        var pathStartIndex = this.serviceUrl.indexOf('/', 6);//'wss://'.length);
        var path = this.serviceUrl.substr(pathStartIndex);
        var portStartIndex = this.serviceUrl.indexOf(':', 6);
        var hostEndIndex = pathStartIndex - 1;
        if(portStartIndex < 0 || portStartIndex > pathStartIndex + 1) {
            //no ':' or ':' in path
            hostEndIndex = pathStartIndex;
        }else{
            hostEndIndex = portStartIndex;//':'
            portStartIndex++;
            var definedPort = this.serviceUrl.substr(portStartIndex, pathStartIndex - portStartIndex);
            log('port:', definedPort);
            definedPort = Number(definedPort);
        }

        if(this.serviceUrl.substr(0, 3) == 'wss'){
            var host = this.serviceUrl.substr(0, hostEndIndex);
            var port = definedPort || 443;
        }else{
            var host = this.serviceUrl.substr(0, hostEndIndex);
            var port = definedPort || 80;
        }
        this.ws = new WebSocket();
        log('to connect:', host, port, path);
        this.ws.on('open', this._onWsOpenFn);
        this.ws.on('close', this._onWsCloseFn);
        this.ws.on('error', this._onWsErrorFn);
        this.ws.on('message', this._onWsMessageFn);
        this.ws.connect(host, port, path);
    }

    _onWsOpen(){
        this.connected = true;
        this.emit('connected');
    }

    _onWsClose(){
        this._onDisconnected();
    }

    _onWsError(error){
        this._onDisconnected(error);
    }

    _onWsMessage(message){
        var str = message.toString();
        log("<", str);
        var reply = JSON.parse(str);
        if(this._willDrop(reply.iflyos_meta))
        {
            log('drop responses for old request id', reply.iflyos_meta.request_id, str);
            return;
        }

        var rid = reply.iflyos_meta.request_id || '';

        /**
         * the responses for a request(id) received
         * @event EVSClient#response_recv_end
         */
        if(reply.iflyos_meta && reply.iflyos_meta.is_last){
            this.emit('response_recv_end', reply.iflyos_meta.request_id);
        }

        var responses = reply.iflyos_responses;
        responses.forEach(rsp=>{
            rsp.header.request_id = rid;
            if(!this._willDrop(reply.iflyos_meta)) {
                this.directiveEmitter.emit(rsp.header.name, rsp);
            }else{
                log('drop responses for old request id', reply.iflyos_meta.request_id, str);
            }
        });

        /**
         * the responses for a request(id) emitted
         * @event EVSClient#response_proc_end
         */
        if(reply.iflyos_meta && reply.iflyos_meta.is_last){
            this.emit('response_proc_end', reply.iflyos_meta.request_id);
        }
    }

    _onDisconnected(error){
        log('disconnected:error?', error);
        if(this.connected) {
            this.connected = false;
            this.emit('disconnected');
            if(this.ws) {
                this.ws.removeListener('open', this._onWsOpenFn);
                this.ws.removeListener('close', this._onWsCloseFn);
                this.ws.removeListener('error', this._onWsErrorFn);
                this.ws.removeListener('message', this._onWsMessageFn);
                try {
                    this.ws.close();
                }catch(e){

                }
            }
            if (this.running) {
                //if running, auto reconnect
                this.reconnect();
            }
        }
    }

    _willDrop(meta){
        /**
         * current we're doing manual request, the server response a request_id is a manual request (start with 'manual').
         * we drop non-ongoing response
         */
        //log('willDrop?', this.recogReqId, meta.request_id);
        return this.recogReqId && this.recogReqId[0] == "m"
            && meta && meta.request_id && meta.request_id[0] == "m" && meta.request_id != this.recogReqId;
    }


    /**
     * disconnect from server
     */
    stop(){
        log('stop()');
        this.running = false;
        if(!this.connected) return;
        log('to close websocket');
        this.ws.close();
        this._onDisconnected('user close');
    }

    /**
     * reconnect(close and connect again) to server
     */
    reconnect(){
        log('reconnect()');
        this.stop();
        setTimeout(()=>{
            this.startServerConnection();
        }, 5000);
    }


    /**
     * send request, refer to iFLYOS doc for request details
     * @param req the request
     * @param req.header request header
     * @param req.header.name request name, like `recognizer.audio_in`
     * @param req.payload request payload
     * @param [context] the context, if missing, get one from context mgr
     */
    request(req, context) {
        if (req.header && !req.header.request_id) {
            req.header.request_id = EVSClient._uuid();
        }

        var p;
        if(context){
            p = Promise.resolve(context);
        } else {
            p = this.cxtMgr.getContext();
        }

        return p.then(c=>{
            var str = JSON.stringify({
                iflyos_header: this.deviceInfo,
                iflyos_context: c,
                iflyos_request: req
            });
            log('>', str);
            this.ws.send(str);
        });
    }

    static _uuid(){
        var buf = fs.readFileSync('/proc/sys/kernel/random/uuid');
        return "auto-" + buf.toString(-1, 0, buf.length - 1);
    }

    /**
     * send voice data to service
     * @param {Buffer}voice
     */
    writeVoice(voice){
        this.ws.send(voice, {binary:true});
    }

    /**
     * end current writing voice stream
     */
    endVoice(){
        log('> __END__');
        this.ws.send("__END__");
    }

    cancelVoice(){
        log('> _CANCEL_');
        this.ws.send("__CANCEL__");
    }

    /**
     * set new request id
     * @param {string}id new request id
     */
    setCurrentRequestId(id){
        log('start new request id', id);
        this.recogReqId = id;
    }

    /**
     * set new access token
     * @param {string}token new access token
     */
    updateToken(token) {
        this.token = token;
        // var tokenStartIdx = this.serviceUrl.indexOf('?token=');
        // var tokenEndIdx = this.serviceUrl.indexOf('&device_id=', tokenStartIdx);
        // this.serviceUrl = this.serviceUrl.substr(0, tokenStartIdx) + '?token=' + token + this.serviceUrl.substr(tokenEndIdx);
        this.serviceUrl = this.wssUrl + "?token=" + this.token + "&device_id=" + this.escapedId;
        this.deviceInfo = {
            authorization: "Bearer " + this.token,
            device:{
                device_id: this.deviceId,
                //     "ip": "xxx.xxx.xxx.xxx",
                //     "location": {
                //         "latitude": 132.56481,
                //         "longitude": 22.36549
                //     },
                platform: {
                    name: "linux",
                    version: "ubuntu 19.04"
                }
            }
        };
    }

    getToken(){
        return this.token;
    }

    /**
     * register directive handler
     * @param {string}directive
     * @param handler
     */
    onDirective(directive, handler){
        this.directiveEmitter.on(directive, handler);
    }

    /**
     * unregister directive handler
     * @param {string}directive
     * @param handler
     */
    removeDirectiveHandler(directive, handler){
       this.directiveEmitter.removeListener(directive, handler);
    }

    /**
     * update device info
     * @param info device info
     * @param info.ip ip address
     * @param info.location.latitude location latitude
     * @param info.location.longitude location longitude
     */
    updateDeviceInfo(info){
        this.deviceInfo.device = Object.assign(this.deviceInfo.device, info);
    }
}


exports.EVSClient = EVSClient;