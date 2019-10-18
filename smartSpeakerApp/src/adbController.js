var iFlyosUtil = require('iflyos_util');
var Log = require('console').Log;
var fs = require('fs');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('adb'));

function temporaryEnable(){
    fs.writeFile('/tmp/enable-adb','dummy', (err)=>{
        log('temporaryEnable. err?', err);
        iFlyosUtil.system('/etc/init.d/S89usbgadget start');
    })
}

function persistEnable(){
    fs.writeFile('/data/enable-adb','dummy', (err)=>{
        log('persistEnable. err?', err);
    })
}

module.exports.temporaryEnable = temporaryEnable;
module.exports.persistEnable = persistEnable;
