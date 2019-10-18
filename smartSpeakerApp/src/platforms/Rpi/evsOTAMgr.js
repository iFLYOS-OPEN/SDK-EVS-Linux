var Log = require("console").Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('OTAMgr'));


/**
 * EVS ota manager implementation
 * @class
 */
class EVSOTAMgr extends EventEmitter  {
    /**
     * Create EVSNetworkMgr instance
     * @param {json} cfg
     */
    constructor(cfg) {
        super();
    }

    start() {
        this.emit('status', 'ready');
    }

    otaCheck() {

    }
}

module.exports.EVSOTAMgr = EVSOTAMgr;
