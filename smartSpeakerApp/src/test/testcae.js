var Voice = require('../audio').Voice;
var console = require('console');

voice = new Voice({
    caeResPath: '/data/cae0809/lanxiaofei.bin'
});
voice.enableWakeup(true);
voice.enableCapture(true);
voice.on('wakeup', function (params){
    console.log('wakeup', JSON.stringify(params));
});

setInterval(()=>{
    // var i = 0;
    // while(true){
    //     if(i % (1024 * 1024) == 0){
    //         console.log('i:',i);
    //     }
    //     i++;
    // }
}, 10000);