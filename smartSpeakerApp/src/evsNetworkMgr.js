var EventEmitter = require('events');

var EVSBleControl = require('./platforms/' + iflyosPlatform + '/evsBleControl').EVSBleControl;
var EVSWifiControl = require('./platforms/' + iflyosPlatform + '/evsWifiControl').EVSWifiControl;
var Log = require('console').Log;

var log;
(function(logger){
    log = logger.log.bind(logger);
})(new Log('NetworkMgr'));

/**
 * EVS network configuration Manager implementation
 * @class
 */
class EVSNetworkMgr extends EventEmitter  {
    /**
     * Create EVSNetworkMgr instance
     * @param {json} cfg - configurations
     */
    constructor(cfg) {
        super();

        this.serverUuid = '00001FF9-0000-1000-8000-00805F9B34FB';
        this.characUuid = '00001FFA-0000-1000-8000-00805F9B34FB';
        this.acceptTimeout = 120000;
        this.dataTimeout = 120000;

        this.acceptTimer = null;
        this.dataTimer = null;

        this.ble = null;
        this.wifiAdapter = null;
        this.clientId = cfg.clientID.split('-').join('');
        this.deviceId = cfg.deviceID;
        this.bleName = cfg.bleName;
    }

    /**
     * Start to configure network
     * @fires EVSNetworkMgr#status
     * @fires EVSNetworkMgr#error
     * @fires EVSNetworkMgr#ok
     */
    start() {
        if (!this.ble) {
            this.ble = new EVSBleControl(this.clientId, this.deviceId, this.bleName, this.serverUuid, this.characUuid);
            this.ble.on('accept', this._onAccept.bind(this));
            this.ble.on('disconnect', this._onDisconnect.bind(this));
            this.ble.on('data', this._onData.bind(this));
        }

        if (this.acceptTimer) {
            clearTimeout(this.acceptTimer);
            this.acceptTimer = null;
        }

        this.ble.start();
        this.acceptTimer = setTimeout(this._onAcceptTimeout.bind(this), this.acceptTimeout);
        this.emit('status', 'ready');
    }

    /**
     * stop network configuration
     */
    stop() {
        if (this.acceptTimer) {
            clearTimeout(this.acceptTimer);
            this.acceptTimer = null;
        }

        if (this.dataTimer) {
            clearTimeout(this.dataTimer);
            this.dataTimer = null;
        }

        if (this.ble) {
            this.ble.stop();
        }
    }

    _onAccept() {
        clearTimeout(this.acceptTimer);
        this.acceptTimer = null;

        this.dataTimer = setTimeout(this._onDataTimeout.bind(this), this.dataTimeout);
        this.emit('status', 'accept');
    }

    _onAcceptTimeout() {
        this.acceptTimer = null;
        this.emit('fail', new Error('accept_timeout'));
    }

    _onData(ssid, password, code) {
        log('receive: ssid=' + ssid);
        log('receive: password=' + password);
        log('receive: code=' + code);

        clearTimeout(this.dataTimer);
        this.dataTimer = null;
        log('===============111');
        this.ble.disconnect();

        if (!this.wifiAdapter) {
            this.wifiAdapter = new EVSWifiControl();
        }

        if (!ssid || !code) {
            this.emit('fail', new Error('wrong_data'));
        } else {
            this.emit('status', 'config');
            this.wifiAdapter.configWifi(ssid, password, (error) => {
                if (error) {
                    this.emit('fail', new Error('config_fail'));
                } else {
                    this.emit('ok', code);
                }
            });
        }
    }

    _onDataTimeout() {
        this.dataTimer = null;
        this.ble.disconnect();
        this.emit('fail', new Error('data_timeout'));
    }

    _onDisconnect() {
        // ble client actively disconnect
        if (this.dataTimer) {
            clearTimeout(this.dataTimer);
            this.dataTimer = null;
            this.emit('fail', new Error('disconnect'));
        }
    }
}

/**
 * status event
 * @event EVSNetworkMgr#status
 * @type {string} status
 */

/**
 * error event
 * @event EVSNetworkMgr#error
 * @type {object} an Error type object
 */

/**
 * ok event
 * @event EVSNetworkMgr#ok
 * @type {object} authorization code
 */

module.exports.EVSNetworkMgr = EVSNetworkMgr;
