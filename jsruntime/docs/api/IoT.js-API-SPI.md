### Platform Support

The following shows spi module APIs available for each platform.

|  | Linux<br/>(Ubuntu) | Tizen<br/>(Raspberry Pi) | Raspbian<br/>(Raspberry Pi) | NuttX<br/>(STM32F4-Discovery) | TizenRT<br/>(Artik053) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| spi.open | X | O | O | O | O |
| spi.openSync | X | O | O | O | O |
| spibus.transfer | X | O | O | O | O |
| spibus.transferSync | X | O | O | O | O |
| spibus.close | X | O | O | O | O |
| spibus.closeSync | X | O | O | O | O |


## Class: SPI

SPI (Serial Peripheral Interface) is a communication protocol which defines a way to communicate between devices.

* On Tizen, the bus number is defined in [this documentation](../targets/tizen/SystemIO-Pin-Information-Tizen.md#spi).
* On NuttX, you have to know the number of pins that is defined on the target board module. For more information, please see the list below.
  * [STM32F4-discovery](../targets/nuttx/stm32f4dis/IoT.js-API-Stm32f4dis.md)

### SPI.MODE
The clock polarity and the clock phase can be specified as `0` or `1` to form four unique modes to provide flexibility in communication between devices. The `SPI.MODE` will specify which one to use (the combinations of the polarity and phase).

* `0` Clock Polarity(0), Clock Phase(0), Clock Edge(1)
* `1` Clock Polarity(0), Clock Phase(1), Clock Edge(0)
* `2` Clock Polarity(1), Clock Phase(0), Clock Edge(1)
* `3` Clock Polarity(1), Clock Phase(1), Clock Edge(0)

### SPI.CHIPSELECT
* `NONE`
* `HIGH`

The chip select is an access-enable switch. When the chip select pin is in the `HIGH` state, the device responds to changes on its input pins.

### SPI.BITORDER
* `MSB` The most significant bit.
* `LSB` The least significant bit.

Sets the order of the bits shifted out of and into the SPI bus, either MSB (most-significant bit first) or LSB (least-significant bit first).

### spi.open(configuration, callback)
* `configuration` {Object}
  * `device` {string} The specified path for `spidev`. (only on Linux)
  * `bus` {number} The specified bus number. (Tizen, TizenRT and NuttX only)
  * `mode` {SPI.MODE} The combinations of the polarity and phase. **Default:** `SPI.MODE[0]`.
  * `chipSelect` {SPI.CHIPSELECT} Chip select state. **Default:** `SPI.CHIPSELECT.NONE`.
  * `maxSpeed` {number} Maximum transfer speed. **Default:** `500000`.
  * `bitsPerWord` {number} Bits per word to send (should be 8 or 9). **Default:** `8`.
  * `bitOrder` {SPI.BITORDER} Order of the bits shifted out of and into the SPI bus. Default: `SPI.BITORDER.MSB`.
  * `loopback` {boolean} Using loopback. **Default:** `false`.
* `callback` {Function}.
  * `err` {Error|null}.
  * `spibus` {Object} An instance of SPIBus.
* Returns: {Object} An instance of SPIBus.

Opens an SPI device with the specified configuration.

**Example**

```js

var spi = require('spi');
var spi0 = spi.open({
  device: '/dev/spidev0.0'
  }, function(err) {
    if (err) {
      throw err;
    }
});

```

### spi.openSync(configuration)
* `configuration` {Object}
  * `device` {string} The specified path for `spidev`. (only on Linux)
  * `bus` {number} The specified bus number. (Tizen, TizenRT and NuttX only)
  * `mode` {SPI.MODE} The combinations of the polarity and phase. **Default:** `SPI.MODE[0]`.
  * `chipSelect` {SPI.CHIPSELECT} Chip select state. **Default:** `SPI.CHIPSELECT.NONE`.
  * `maxSpeed` {number} Maximum transfer speed. **Default:** `500000`.
  * `bitsPerWord` {number} Bits per word to send (should be 8 or 9). **Default:** `8`.
  * `bitOrder` {SPI.BITORDER} Order of the bits shifted out of and into the SPI bus. Default: `SPI.BITORDER.MSB`.
  * `loopback` {boolean} Using loopback. **Default:** `false`.
* Returns: {Object} An instance of SPIBus.

Opens an SPI device with the specified configuration.

**Example**

```js

var spi = require('spi');
var spi0 = spi.openSync({
  device: '/dev/spidev0.0'
});

```

## Class: SPIBus

The SPIBus is commonly used for communication.

### spibus.transfer(txBuffer, [, callback])
* `txBuffer` {Array|Buffer}.
* `callback` {Function}.
  * `err` {Error|null}.
  * `rxBuffer` {Array}.

Writes and reads data from the SPI device asynchronously.
The `txBuffer` and `rxBuffer` length is equal.

**Example**

```js

var tx = new Buffer('Hello IoTjs');
var rx = new Buffer(tx.length);
spi0.transfer(tx, function(err, rx) {
  if (err) {
    throw err;
  }

  var value = '';
  for (var i = 0; i < tx.length; i++) {
    value += String.fromCharCode(rx[i]);
  }
  console.log(value);
});

```

### spibus.transferSync(txBuffer)
* `txBuffer` {Array|Buffer}.
* Returns: `rxBuffer` {Array}.

Writes and reads data from the SPI device synchronously.
The `txBuffer` and `rxBuffer` length is equal.

**Example**

```js

var tx = new Buffer('Hello IoTjs');
var rx = spi0.transferSync(tx);
var value = '';
for (var i = 0; i < tx.length; i++) {
  value += String.fromCharCode(rx[i]);
}
console.log(value);

```

### spibus.close([callback])
* `callback` {Function}.
  * `err` {Error|null}.

Closes the SPI device asynchronously.

The `callback` function will be called after the SPI device is closed.

**Example**
```js

spi0.close(function(err) {
  if (err) {
    throw err;
  }
  console.log('spi bus is closed');
});

```

### spibus.closeSync()

Closes the SPI device synchronously.

**Example**
```js

spi.closeSync();

console.log('spi bus is closed');

```
