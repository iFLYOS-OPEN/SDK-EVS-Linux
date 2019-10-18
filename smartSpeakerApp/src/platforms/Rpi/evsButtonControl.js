var Log = require('console').Log;
var EventEmitter = require('events');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('ButtonControl'));


/**
 * @class
 * Button control implementation
 */
class EVSButtonControl extends EventEmitter {
    constructor() {
        super();
    }
}

module.exports.EVSButtonControl = EVSButtonControl;