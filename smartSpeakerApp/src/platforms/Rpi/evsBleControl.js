var EventEmitter = require('events');

/**
 * EVS native ble control implementation
 * @class
 */
class EVSBleControl extends EventEmitter {
    /**
     * Create EVSBleControl instance
     * @param {string} clientId
     * @param {string} deviceId
     * @param {string} bleName
     * @param {string} serverUuid
     * @param {string} charUuid
     */
    constructor(clientId, deviceId, bleName, serverUuid, charUuid) {
        super();

        this.advData = EVSBleControl._generateAvdData(clientId);
        this.scanData = EVSBleControl._generateScanData(bleName);
        this.readData = new Buffer(deviceId);

        this.bleRunning = false;
        this.bleReady = false;
        this.bleReadyCallback = null;

        this.ble = require('ble');
        var BlenoPrimaryService = this.ble.PrimaryService;
        var BlenoCharacteristic = this.ble.Characteristic;

        this.data = '';

        this.readCallback = function (offset, callback) {
            callback(BlenoCharacteristic.RESULT_SUCCESS, this.readData);
        };

        this.writeCallback = function (data, offset, withoutResponse, callback) {
            this._onData(data);
            callback(BlenoCharacteristic.RESULT_SUCCESS);
        };

        this.character = new BlenoCharacteristic({
            uuid: charUuid,
            properties: ['read', 'write', 'writeWithoutResponse'],
            secure: [],
            value: null,
            descriptors: [],
            onReadRequest: this.readCallback.bind(this),
            onWriteRequest: this.writeCallback.bind(this),
            onSubscribe: null,
            onUnsubscribe: null,
            onNotify: null,
            onIndicate: null
        });

        this.service = new BlenoPrimaryService({
            uuid: serverUuid,
            characteristics: [
                this.character
            ]
        });

        this.ble.on('stateChange', this._onStateChange.bind(this));
        this.ble.on('advertisingStart', this._onAdvertisingStart.bind(this));
        this.ble.on('accept', this._onAccept.bind(this));
        this.ble.on('disconnect', this._onDisconnect.bind(this));
    }

    /**
     * Start ble advertising
     * @fires EVSBleControl#accept
     * @fires EVSBleControl#data
     * @fires EVSBleControl#disconnect
     * @fires EVSBleControl#error
     */
    start() {
        if (this.bleRunning) {
            return;
        } else if (this.bleReady) {
            this.ble.startAdvertisingWithEIRData(this.advData, this.scanData);
            this.bleReadyCallback = null;
            this.bleRunning = true;
        } else {
            this.bleReadyCallback = this.start.bind(this);
        }
    }

    /**
     * Stop ble advertising
     */
    stop() {
        this.bleReadyCallback = null;
        if (this.bleRunning) {
            this.ble.disconnect();
            this.ble.stopAdvertising();
            this.bleRunning = false;
        }
    }

    /**
     * Disconnect current connection
     */
    disconnect() {
        this.ble.disconnect();
    }

    _onStateChange(state) {
        if (state === 'poweredOn') {
            this.bleReady = true;
            if (this.bleReadyCallback) {
                this.bleReadyCallback();
            }
        }
    }

    _onAdvertisingStart(error) {
        if (!error) {
            this.ble.setServices([
                this.service
            ]);
        } else {
            console.log('error while starting advertising:', error);

            /**
             * Fail to start advertising
             * @event EVSBleControl#data
             */
            this.emit('error');
        }
    }

    _onAccept(clientAddress) {
        /**
         * New ble connection
         * @event EVSBleControl#accept
         */
        this.emit('accept');
    }

    _onData(data) {
        var str = data.toString();
        if (str !== 'keep-alive' && str !== 'disconnect') {
            var parseInfo = (dataArray) => {
                var netInfo = {'id': '', 'pwd': '', 'code': ''};
                dataArray.forEach(function (v, i) {
                    Object.keys(netInfo).forEach(function (key) {
                        if (v.substr(0, key.length) === key) {
                            netInfo[key] = v.substr(key.length + 1);
                        }
                    });
                });
                
                /**
                 * receive data from ble client
                 * @event EVSBleControl#data
                 * @type {string} wifi ssid
                 * @type {string} wifi password
                 * @type {string} authorization code
                 */
                this.emit('data', netInfo.id, netInfo.pwd, netInfo.code);
            };

            this.data += str;
            var dataArray = this.data.split('\n');
            if (dataArray[0].substr(0, 3) === 'ver') {
                if (dataArray[dataArray.length - 1] === 'end') {
                    parseInfo(dataArray);
                }
            } else if (dataArray.length === 3) {
                parseInfo(dataArray);
            }
        }
    }

    _onDisconnect(clientAddress) {
        /**
         * disconnect from ble client
         * @event EVSBleControl#disconnect
         */
        this.emit('disconnect');
    }

    static _generateAvdData(clientId) {
        var clientIdHex = new Buffer(clientId.split('-').join(''), 'hex');
        var advData = new Buffer(27);
        advData.writeUInt8(0x02, 0);
        advData.writeUInt8(0x01, 1);
        advData.writeUInt8(0x1E, 2);
        advData.writeUInt8(0x13, 3);
        advData.writeUInt8(0xFF, 4);
        advData.writeUInt8(0xAA, 5);
        advData.writeUInt8(0xAA, 6);
        clientIdHex.copy(advData, 7);
        advData.writeUInt8(0x03, 23);
        advData.writeUInt8(0x03, 24);
        advData.writeUInt8(0xF9, 25);
        advData.writeUInt8(0x1F, 26);
        return advData;
    }

    static _generateScanData(name) {
        var bleName = new Buffer(name);
        var scanData = new Buffer(2 + name.length);
        scanData.writeUInt8(1 + name.length, 0);
        scanData.writeUInt8(0x08, 1);
        bleName.copy(scanData, 2);
        return scanData;
    }
}

module.exports.EVSBleControl = EVSBleControl;
