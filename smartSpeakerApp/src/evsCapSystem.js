var Log = require('console').Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('CapSystem'));

/**
 * @class
 * EVS System capability implementation
 */
class EVSCapSystem extends EventEmitter{
    /**
     * @param {EVSClient}client
     * @param {EVSContextMgr}cxtMgr
     * @fires EVSCapSystem#ping_timeout
     * @fires EVSCapSystem#time_changed
     * @fires EVSCapSystem#response_error
     * @fires EVSCapSystem#device_removed
     */
    constructor(client, cxtMgr) {
        super();
        cxtMgr.addContextProvider(this._getContext.bind(this));
        this.pingTimeout = 1000 * 60 * 2;
        this.client = client;
        this.pingCheckTimer = undefined;
        this.syncContextTimer = undefined;
        this.client.onDirective("system.ping", this._onPing.bind(this));
        this.client.onDirective("system.error", this._onError.bind(this));
        this.client.onDirective("system.revoke_authorization", this._onRevokeAuth.bind(this));
        this.client.on('connected', this._onConnected.bind(this));
        this.client.on('disconnected', this._onDisconnected.bind(this));
    }

    _getContext() {
        return {
            system:{
                version:"1.0"
            }
        };
    }

    /**
     * store exception to server. refer to iFLYOS doc for details
     * @param {string}type
     * @param {string}code
     * @param {string}message
     */
    sendException(type, code, message){
        log('client has exception:', type, code, message);
        this.client.request({
            header:{
                name: 'system.exception'
            },
            payload:{
                type: type,
                code: code,
                message: message
            }
        });
    }

    _onRevokeAuth(response){
        /**
         * device's authentication has been removed
         * @event EVSCapSystem#device_removed
         */
        this.emit('device_removed');
    }

    _onError(response){
        log('response error:', response);
        /**
         * server responses a system.error
         * @event EVSCapSystem#response_error
         */
        this.emit('response_error', response);
    }

    /**
     * sync latest device context to server
     * @param [reason] why we need sync
     */
    syncContext(reason){
        log('to send system.state_sync because?', reason);
        this.client.request({
            header:{
                name: "system.state_sync"
            },
            payload:{

            }
        });
        log('to sync context 15 mins later');
        if(this.syncContextTimer){
            clearTimeout(this.syncContextTimer);
        }
        this.syncContextTimer = setTimeout(()=>{
            this.syncContext("15 min cycle");
        }, 15 * 60 * 1000);
    }

    _onPing(response){
        if(this.pingCheckTimer){
            clearTimeout(this.pingCheckTimer);
            this.pingCheckTimer = undefined;
        }
        log('timestamp from server', response.payload.timestamp);
        var timeDiff = Date.now() - response.payload.timestamp * 1000;
        if(timeDiff > 60000 || timeDiff < -60000){
            /**
             * local time is different from server
             * @event EVSCapSystem#time_changed
             */
            log('fire time_changed');
            this.emit('time_changed', response.payload.timestamp * 1000);
        }
        log('reset ping timeout');
        this.pingCheckTimer = setTimeout(this._onPingCheckTimeout.bind(this), this.pingTimeout + 5000);
    }

    _onPingCheckTimeout(){
        /**
         * timeout receiving ping from server
         * @event EVSCapSystem#ping_timeout
         */
        log("receiving ping timeout");
        this.pingCheckTimer = undefined;
        this.emit('ping_timeout');
        this.client.reconnect();
    }

    _onConnected(){
        if(this.syncContextTimer){
            clearTimeout(this.syncContextTimer);
            this.syncContextTimer = undefined;
        }
        this.syncContextTimer = setTimeout(()=>{
            this.syncContext('client startup');
        }, 0);
        if(this.pingCheckTimer){
            clearTimeout(this.pingCheckTimer);
            this.pingCheckTimer = undefined;
        }
        log('evs connected, start ping timeout check');
        this.pingCheckTimer = setTimeout(this._onPingCheckTimeout.bind(this), this.pingTimeout + 5000);
    }

    _onDisconnected(){
        log('evs disconnected, stop ping timeout check');
        if(this.pingCheckTimer){
            clearTimeout(this.pingCheckTimer);
            this.pingCheckTimer = undefined;
        }

        log('evs disconnected, stop interval context sync');
        if(this.syncContextTimer){
            clearTimeout(this.syncContextTimer);
            this.syncContextTimer = undefined;
        }
    }
}

exports.EVSCapSystem = EVSCapSystem;