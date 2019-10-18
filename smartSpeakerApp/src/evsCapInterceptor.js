var Log = require('console').Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('CapInterceptor'));

/**
 * @class
 * EVS Interceptor capability implementation
 */
class EVSCapInterceptor extends EventEmitter{
    /**
     *
     * @param {EVSClient}client
     * @param {EVSContextMgr}cxtMgr
     * @param {EVSCapSystem}capSystem
     * @fires EVSCapInterceptor#aiui
     * @fires EVSCapInterceptor#custom
     */
    constructor(client, cxtMgr, capSystem){
        super();
        cxtMgr.addContextProvider(this._getContext.bind(this));
        client.onDirective('interceptor.aiui', this._onAIUI.bind(this));
        client.onDirective('interceptor.custom', this._onCustom.bind(this));
    }

    _onAIUI(response){
        /**
         * aiui interceptor response
         * @event EVSCapInterceptor#aiui
         */
        this.emit('aiui', response);
    }

    _onCustom(response){
        /**
         * custom interceptor response
         * @event EVSCapInterceptor#custom
         */
        this.emit('custom', response);
    }

    _getContext(){
        return {
            interceptor: {
                version: "1.0"
            }
        };
    }
}

exports.EVSCapInterceptor = EVSCapInterceptor;