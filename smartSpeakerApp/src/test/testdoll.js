var DollMgr = require('../dollMgr').DollMgr;

class MockClient {
    getToken(){
        return 'SJEaYzsTTq9-fwGeMpQLqHbOedCsqvEw6Ur_evjkzpQ5G0DgFXWvIHvH9poTXdR0';
    }
}

var mgr = new DollMgr(new MockClient());

mgr.on('init', (info)=>{
    if(info && info.hiSoundFile){
        console.log('初始化-玩偶可用', JSON.stringify(info));
    }else if(info == 'not_present') {
        console.log('初始化-没有玩偶');
        mgr.monitorPlugin();
    }else {
        console.log('初始化-有玩偶-需要下载资源');
        mgr.startDownloadRes();
    }
});

mgr.on('preparing', ()=>{
    console.log('正在下载资源');
});

mgr.on('cancel', (err)=>{
    if(err == 'doll'){
        console.log('玩偶移除-取消下载', err);
    }else{
        console.log('网络问题或服务端拒绝', err);
    }
    mgr.unMonitorPlugin();
    mgr.monitorPlugin();
});

mgr.on('doll_unset', (info)=>{
    console.log('玩偶移除-', JSON.stringify(info));
    mgr.unMonitorPlugin();
    mgr.monitorPlugin();
});

mgr.on('doll_set', (info)=>{
    console.log("玩偶可用-", JSON.stringify(info));
});

mgr.start();

setInterval(()=>{console.log('1sec tick');}, 1000);
