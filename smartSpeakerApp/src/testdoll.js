
var iflyos = require('iflyos');
var router = iflyos.router || iflyos.initIPC();

function configWifi() {
    var ssid = 'iot test';
    var password = '12345678';
    console.log('===========================config wifi');
    router.send('wireless', 'EVT_CFG_WIFI', [ssid, password]);
}

// var aa = function(b) {
//     b = 10;
// };
//
// var c = 1;
// aa(c);
// console.log(c);
//
// var util = require('iflyos_util');
// var EVSOTAMgr = require('./platforms/A113X/evsOTAMgr').EVSOTAMgr;
//
// var clientId = '54f8873c-1db4-4588-9bd3-9eaa30149fec';
// var deviceId = '200016011136050';
// var clientSecret = '56f8f23944ac695996aa60c3dab294b8f0e7a030';
// var otaMgr = new EVSOTAMgr(clientId, deviceId, clientSecret);
// otaMgr.check();
//
// setInterval(()=>{}, 10000);

var util = require('util');
var EventEmitter = require('events');

class EventCreater extends EventEmitter {
    constructor() {
        super();

        setInterval(() => {
            this.emit('ok');
        }, 2000);
    }
}

class BaseEv extends EventEmitter {
    constructor() {
        super();

        this.evCreator = new EventCreater();
        this.evCreator.on('ok', this._onOK.bind(this));

        this.mode = null;
        this.evMap = {};
    }

    on(mode, type, listener) {
        if (!this.evMap[type]) {
            this.evMap[type] = {};
        }

        if (!this.evMap[type][mode]) {
            this.evMap[type][mode] = [];
        }

        this.evMap[type][mode].push(listener);
    }

    switchMode() {
        if (!this.mode || this.mode === 'B') {
            this.mode = 'A'
        } else {
            this.mode = 'B';
        }

        console.log('mode is changed to', this.mode);
    }

    _onOK() {
        if (this.mode) {
            var listeners = this.evMap.ok[this.mode];
            if (util.isArray(listeners)) {
                listeners = listeners.slice();
                var args = Array.prototype.slice.call(arguments, 1);
                for (var i = 0; i < listeners.length; ++i) {
                    // if(type=='wakeup'){
                    //   console.log('wakeup args length', args.length);
                    //   console.log('wakeup args[0]', JSON.stringify(args[0]));
                    // }
                    listeners[i].apply(this, args);
                }
            }
        }
    }
}

var ev = new BaseEv();

ev.on('A', 'ok', ()=>{
    console.log('it is A');
});

ev.on('B', 'ok', ()=>{
    console.log('it is B');
});

setInterval(()=> {
    ev.switchMode();
}, 5000);


