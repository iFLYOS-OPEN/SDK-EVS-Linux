var fs = require('fs');
var LEDControl = require('../platforms/'+iflyosPlatform+'/iflyos_led').LEDControl;
var led;

var proc = require('process');
var argv = proc.argv.slice(1);
var led = new LEDControl((err)=>{
    if(err){
        throw err;
    }
    led.wakeup(60);
    setTimeout(()=>{
        led.wakeup(60);
    }, 100);
});

setTimeout(()=>{
    console.log('led', led.toString());
}, 20000);