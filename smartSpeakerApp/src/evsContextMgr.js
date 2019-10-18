var Log = require('console').Log;
var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('EvsCxtMgr'));


/**
 * @class EVSContextMgr
 * device context manager. Response to collect context
 */
class EVSContextMgr {
    /**
     * @param {string}token access token
     */
    constructor(){
        //this.token = token;
        this.providers = [];
    }

    /**
     * register device context provider, refer to iFLYOS doc for more context details
     *
     * @param {function}provider the context provider, which return a context
     */
    addContextProvider(provider){
        this.providers.push(provider);
    }

    /**
     * get context for sending request, value for `iflyos_context`
     */
    getContext() {
        log('getContext()');
        var obj = {
            cloud_alarms:{
                version:"1.0"
            }
        };
        var promises = [];
        this.providers.map(p=>{
            var c = p();
            if(!c.then){
                //log('resolve directly value', c);
                obj = Object.assign(obj, c);
            }else{
                promises.push(c);
            }
        });
        if(promises.length == 0){
            return Promise.resolve(obj);
        }

        log('to resolve', promises.length, 'async context');
        return new Promise(r=>{
            Promise.all(promises).then(cs=>{
                cs.forEach(c=>{
                    //log('resolve', c);
                    obj = Object.assign(obj, c);
                });
                r(obj);
            });
        });
    }
}

exports.EVSContextMgr = EVSContextMgr;