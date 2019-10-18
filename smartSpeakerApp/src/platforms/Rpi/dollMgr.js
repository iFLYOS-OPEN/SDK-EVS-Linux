var EventEmitter = require('events');
var Log = require('console').Log;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('dollMgr'));

class DollMgr extends EventEmitter{
    /**
     *
     * @param {EvsClient}client because token may change, we need client not token here
     */
    constructor(client) {
        super();
        this.client = client;

    }

    start() {
        this.emit('init', 'not_present');
    }
}

module.exports.DollMgr = DollMgr;