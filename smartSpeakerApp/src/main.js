var console = require("console");
console.useFileLog(1024);//1MB per log file
var Log = console.Log;
var proc = require('process');
var fs = require('fs');

var iFlyosUtil = require('iflyos_util');
var iflyos = require('iflyos');

var EVSContextMgr = require('./evsContextMgr').EVSContextMgr;
var EVSAuthorizeMgr = require('./evsAuthorizeMgr').EVSAuthorizeMgr;
var EVSAudioMgr = require('./evsAudioMgr').EVSAudioMgr;
var EVSSoundPlayer = require('./evsSoundPlayer').EVSSoundPlayer;
var InternetMonitor = require("./internetMonitor.js");
var LEDControl = require('./platforms/' + iflyosPlatform + '/iflyos_led').LEDControl;

var EVSModuleStartup = require('./evsModuleStartup').EVSModuleStartup;
var EVSModuleConfig = require('./evsModuleConfig').EVSModuleConfig;
var EVSModuleInteraction = require('./evsModuleInteraction').EVSModuleInteraction;
var EVSModuleLed = require('./evsModuleLed').EVSModuleLed;
var EVSModuleDoll = require('./evsModuleDoll').EVSModuleDoll;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('main'));

var cfg = {};
var argv = proc.argv.slice(1);

// 检查配置文件
function argumentsCheck() {
    if (!argv[1]) {
        log("usage: iotjs", argv[0], "configFile");
        proc.exit();
        return;
    }

    if (!fs.existsSync(argv[1])) {
        log("config file", argv[1], "not exists");
        proc.exit();
        return;
    }

    var cfgStat = fs.statSync(argv[1]);
    if (!cfgStat.isFile()) {
        log(argv[1], "is not a file");
        log("usage: iotjs", argv[0], "configFile");
        proc.exit();
        return;
    }

    var cfgStr = fs.readFileSync(argv[1]).toString();
    cfg = JSON.parse(cfgStr);

    // 检查Client ID
    if (!cfg.clientID || cfg.clientID === "__CLIENT__ID__") {
        log("wrong client id in config", argv[1]);
        proc.exit();
        return;
    }

    // 检查Client Secret
    if (!cfg.clientSecret) {
        log('WARN: client secret not set in config');
    }

    // 检查Device ID
    if (!cfg.deviceID) {
        log('WARN: device id not set in config');
    }

    // 检查数据存放目录
    if (!cfg.dbPath) {
        log('missing dbPath in config');
        proc.exit();
        return;
    }

    // 检查音效文件目录
    if (!cfg.soundDir) {
        //log('WARN: sound dir not set in config');
        cfg.soundDir = '/iflyos/sound_effect';
    }

    log("use config", JSON.stringify(cfg));
}

argumentsCheck();
(cfg.preKill || []).forEach(p=>{
    iFlyosUtil.system('killall -9 ' + p);
});

var mac = null;
var sn = null;
var ledCtrl = null;
var router = iflyos.router || iflyos.initIPC();

function checkDeviceID() {
    if (!cfg.deviceID) {
        cfg.deviceID += mac;
        log('xor-ed client:', cfg.deviceID);
    }
}

// 当系统初始化完成，开始程序初始化操作
function onSystemReady() {
    // 检查device id，若配置文件中未设置则根据mac和sn号生成
    checkDeviceID();

    // 初始化网络监测功能、授权功能
    var netMonitor = new InternetMonitor();
    var authMgr = new EVSAuthorizeMgr(cfg);

    // 初始化音频焦点管理功能、本地音效播放功能
    var audioMgr = new EVSAudioMgr();
    var soundPlayer = new EVSSoundPlayer(audioMgr, cfg.soundDir);

    // 初始化Context Manager
    var cxtMgr = new EVSContextMgr();

    // 初始化各个流程逻辑模块
    var moduleStartup = new EVSModuleStartup(cfg, authMgr, netMonitor, soundPlayer);
    var moduleConfig = new EVSModuleConfig(cfg, authMgr, soundPlayer);
    var moduleInteraction = new EVSModuleInteraction(cfg, moduleStartup, moduleConfig,
        cxtMgr, authMgr, audioMgr, netMonitor, soundPlayer);
    var moduleLed = new EVSModuleLed(moduleStartup, moduleConfig, moduleInteraction, ledCtrl, audioMgr);
    if (iflyosProduct === 'adam') {
        var moduleDoll = new EVSModuleDoll(moduleStartup, moduleConfig, moduleInteraction,
            cxtMgr, authMgr, soundPlayer, ledCtrl);
    }

    moduleStartup.start();
    netMonitor.start();
}

function initNetwork() {
    return new Promise((resolve, reject) => {
        if (iflyosPlatform === 'LinuxHost') {
            resolve('ok');
        } else {
            var macPuller = setInterval(() => {
                mac = iFlyosUtil.getWifiMac('wlan0');
                if (mac) {
                    clearInterval(macPuller);
                    log('wlan mac is', mac);
                    resolve();
                }
            }, 100);
        }
    });
}

function initSN() {
    var snFile = '/data/sn.txt';
    return new Promise((resolve, reject) => {
        fs.readFile(snFile, (err, buf) => {
            if (!err && buf) {
                sn = buf.toString().trim();
                log('sn is', sn);
            }
            resolve();
        });
    });
}

function initLEDCtrl() {
    return new Promise((resolve, reject) => {
        ledCtrl = new LEDControl(() => {
            ledCtrl.startup();
            log('led is ready');
            resolve();
        });
    });
}

function initMediaPlayer() {
    return new Promise((resolve, reject) => {
        var onMediaPlayerReady = function() {
            router.removeListener('EVT_MEDIA_PLAYER_READY', onMediaPlayerReady);
        };

        router.once('EVT_MEDIA_PLAYER_READY', () => {
            onMediaPlayerReady();
            log('mediaplayer is ready');
            resolve();
        });

        if (cfg.playerBin) {
            iFlyosUtil.system('killall ' + cfg.playerBin);
            router.broadcast('DO_START', [cfg.playerBin]);
        }
    });
}

// 等待所有初始化操作完成，运行程序主逻辑
var initList = [initNetwork(), initSN(), initLEDCtrl(), initMediaPlayer()];
Promise.all(initList).then(() => {
    var waitTime = Number(argv[2]) || 5000;
    log('system initialized');
    log('wait', waitTime, " ms for system startup");
    setTimeout(() => {
        onSystemReady();
    }, waitTime);
});

setInterval(()=>{
    //forever running
}, 10000);
