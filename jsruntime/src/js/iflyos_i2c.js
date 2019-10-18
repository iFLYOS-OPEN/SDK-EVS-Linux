var i2c = {
    open: function(filePath, address, callback) {
        var i2cBus = new native(filePath, address, function(err) {
            callback(err, i2cBus);
        });
        return i2cBus;
    },
};

module.exports = i2c;
