var Log = require('console').Log;
var EventEmitter = require('events');
var Sqlite = require('sqlite');
var EVSAudioPlayer = require('evsAudioPlayer');
var EVSAudioMgr = require('evsAudioMgr');
var Sounds = require('evsSoundEffect');

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('CapAlarm'));

/**
 * @class
 * EVS Interceptor capability implementation
 */
class EVSCapAlarm extends EventEmitter{
    /**
     *
     * @param {EVSClient}client
     * @param {EVSAudioMgr} audioMgr
     * @param {EVSCapSystem} capSystem
     * @param {EVSContextMgr} contextMgr
     * @param {string}dbpath alarm local sqlite3 database file path
     */
    constructor(client, contextMgr, capSystem, audioMgr, dbpath){
        super();
        this.audioMgr = audioMgr;
        this.audioMgr.on(EVSAudioMgr.ChannelType.ALERT, this._onFocusChanged.bind(this));
        this.capSystem = capSystem;
        contextMgr.addContextProvider(this._getContext.bind(this));
        this.db = new Sqlite(dbpath);
        this.nextAlarmTimer = undefined;
        this.playingAlert = undefined;
        this.player = new EVSAudioPlayer('', 'capAlarmLocal');
        this.player.on('state', this._onPlayerState.bind(this));
        var initPromise = new Promise( (resolve, reject) => {
            this.db.execute('CREATE TABLE IF NOT EXISTS alerts ( alarm_id TEXT PRIMARY KEY, timestamp INTEGER, url TEXT );',
                (err, result)=>{
                    if(err) {
                        log("!!!!create table failed:", err);
                        reject(err);
                    }else{
                        log("sqlite3 table created");
                        this.db.execute('SELECT * FROM alerts ORDER BY timestamp;',
                            (err, result)=>{
                                if(err){
                                    log("!!!!query table failed:", err);
                                    reject(err);
                                }else{
                                    log("got local alerts:", JSON.stringify(result));
                                    this.alerts = result;
                                    resolve('alarm init ok');
                                }
                            });
                    }
                })
        });
        client.add2InitList(initPromise);
        client.onDirective('alarm.set_alarm', this._onSetAlarm.bind(this));
        client.onDirective('alarm.delete_alarm', this._onDeleteAlarm.bind(this));
    }

    _getContext(){
        var cxt = {
            alarm:{
                version:"1.0",
                local:this.alerts.map(a=>Object.assign({}, {alarm_id:a.alarm_id, timestamp:a.timestamp}))
            }
        };
        if(this.playingAlert){
            cxt.alarm['active']={alarm_id: this.playingAlert.alarm_id};
        }
        return cxt;
    }

    _onSetAlarm(response){
        var now = Date.now();
        var alert = response.payload;
        if(alert.timestamp * 1000 <= now) {
            //if alert in response arrives, play but not store
            return;
        }
        this.alerts.push(alert);
        this.alerts.sort((a,b)=>a.timestamp - b.timestamp);
        //TODO: prevent injection attack
        var sql = 'INSERT INTO alerts (alarm_id, timestamp, url) VALUES ("'
            + alert.alarm_id + '",'
            + alert.timestamp + ',"'
            + alert.url
            + '" );';
        this.db.execute(sql,(err)=>{
            if(err){
                log('!!! insert sqlite3 failed, sql:' + sql);
            }
        });
        this.capSystem.syncContext('alarm inserted');
        this._calcNext();
    }

    _onDeleteAlarm(response){
        var alarm = response.payload;
        if(this.playingAlert && this.playingAlert == alarm.alarm_id){
            this._stopPlaying();
            this.audioMgr.releaseChannel(EVSAudioMgr.ChannelType.ALERT);
            return;
        }
        if(!this.alerts.find(a=>a.alarm_id == alarm.alarm_id)){
            log('alert already deleted/not exists:', alarm.alarm_id);
            return;
        }
        var sql = 'DELETE FROM alerts WHERE alarm_id="'+alarm.alarm_id+'";';
        this.db.execute(sql,(err)=>{
            if(err){
                log('!!! delete sqlite3 failed, sql:' + sql);
            }
        });

        this.alerts = this.alerts.filter(a=>a.alarm_id != alarm.alarm_id);

        this.capSystem.syncContext('alarm deleted');
        this._calcNext();
    }

    _calcNext(){
        if(this.nextAlarmTimer){
            clearTimeout(this.nextAlarmTimer);
            this.nextAlarmTimer = undefined;
        }
        var now = Date.now();
        if(this.alerts.length == 0){
            log('no more alerts');
            return;
        }
        var alert = this.alerts[this.alerts.length - 1];
        var delay = alert.timestamp * 1000 - now;
        if(delay < 0){
            delay = 0;
        }
        log('next alarm will play delay:', delay);
        this.nextAlarmTimer = setTimeout(()=>{
            this._playAlert(alert);
        }, delay);
    }

    _onFocusChanged(status){
        if(status == EVSAudioMgr.FocusState.FOREGROUND) {
            this.capSystem.syncContext('alarm playing');
            this.player.play(this.playingAlert.url, "url");
            this.isPlayingLocalAlertResource = false;
        }else if(status == EVSAudioMgr.FocusState.BACKGROUND){
            this.audioMgr.releaseChannel(EVSAudioMgr.ChannelType.ALERT);
        } else if(status == EVSAudioMgr.FocusState.NONE){
            this._stopPlaying();
        }
    }

    _playAlert(alarm){
        var hasChannel = !!this.playingAlert;
        if(this.playingAlert){
            this._stopPlaying();
        }

        this.alerts = this.alerts.filter(a=>a.alarm_id != alarm.alarm_id);
        var sql = 'DELETE FROM alerts WHERE alarm_id="'+alarm.alarm_id+'"';
        this.db.execute(sql,(err)=>{
            if(err){
                log('!!! delete sqlite3 failed, sql:' + sql);
            }
        });
        this.playingAlert = alarm;
        if(!hasChannel) {
            this.audioMgr.acquireChannel(EVSAudioMgr.ChannelType.ALERT);
        }else{
            this.capSystem.syncContext('alarm playing');
            this.player.play(this.playingAlert.url, "url");
            this.isPlayingLocalAlertResource = false;
        }
    }

    _stopPlaying(){
        this.player.stop();
        this.playingAlert = undefined;
        this.capSystem.syncContext('stop playing alarm');
    }

    _onPlayerState(status){
        log('player status', status);
        if(status == EVSAudioPlayer.PlayerActivity.FINISHED){
            this.audioMgr.releaseChannel(EVSAudioMgr.ChannelType.ALERT);
            if(this.playingAlert){
                this.playingAlert = undefined;
                this.capSystem.syncContext('alarm finished');
            }
        }else if(status == EVSAudioPlayer.PlayerActivity.ERROR){
            this.player.stop();
            if(!this.isPlayingLocalAlertResource) {
                this.isPlayingLocalAlertResource = true;
                setTimeout(() => this.player.play(Sounds.LocalAlert, "mp3"), 100);
            }
        }
    }
}

exports.EVSCapAlarm = EVSCapAlarm;