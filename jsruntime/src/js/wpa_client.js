class WpaClient {
    hasWifi() {
        return native.hasWifi();
    }

    configWifi(ssid, pswd, callback) {
        return native.configWifi(ssid, pswd, callback);
    }
}

module.exports.WpaClient = WpaClient;
