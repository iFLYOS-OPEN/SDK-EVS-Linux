### Platform Support

The following shows GPIO module APIs available for each platform.

|  | Linux<br/>(Ubuntu) | Tizen<br/>(Raspberry Pi) | Raspbian<br/>(Raspberry Pi) | NuttX<br/>(STM32F4-Discovery) | TizenRT<br/>(Artik053) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| gpio.open | X | O | O | O | O |
| gpio.openSync | X | O | O | O | O |
| gpiopin.setDirectionSync | X | O | O | O | O |
| gpiopin.write | X | O | O | O | O |
| gpiopin.writeSync | X | O | O | O | O |
| gpiopin.read | X | O | O | O | O |
| gpiopin.readSync | X | O | O | O | O |
| gpiopin.close | X | O | O | O | O |
| gpiopin.closeSync | X | O | O | O | O |


# GPIO

The GPIO module provides access the General Purpose
Input Output pins of the hardware.

On Linux each pin has a logical number starting from `1`.
The logical number might be different from the physical
pin number of the board. The mapping is available
in the documentation of a given board.

* On Tizen, the pin number is defined in [this documentation](../targets/tizen/SystemIO-Pin-Information-Tizen.md#gpio).
* On NuttX, the pin number is defined in the documentation of the target board. For more information, please check the
following list:
[STM32F4-discovery](../targets/nuttx/stm32f4dis/IoT.js-API-Stm32f4dis.md#gpio-pin)


### DIRECTION
* `IN` Input pin.
* `OUT` Output pin.

An enumeration which can be used to specify the
direction of the pin.


### MODE
* `NONE` None.
* `PULLUP` Pull-up (pin direction must be [`IN`](#direction)).
* `PULLDOWN` Pull-down (pin direction must be [`IN`](#direction)).
* `FLOAT` Float (pin direction must be [`OUT`](#direction)).
* `PUSHPULL` Push-pull (pin direction must be [`OUT`](#direction)).
* `OPENDRAIN` Open drain (pin direction must be [`OUT`](#direction)).

An enumeration which can be used to specify the
mode of the pin. These options are only supported on NuttX.


### EDGE
* `NONE` None.
* `RISING` Rising.
* `FALLING` Falling.
* `BOTH` Both.

An enumeration which can be used to specify the
edge of the pin.


### gpio.open(configuration, callback)
* `configuration` {Object} Configuration for open GPIOPin.
  * `pin` {number} Pin number. Mandatory field.
  * `direction` {[gpio.DIRECTION](#direction)} Pin direction. **Default:** `gpio.DIRECTION.OUT`
  * `mode` {[gpio.MODE](#mode)} Pin mode. **Default:** `gpio.MODE.NONE`
  * `edge` {[gpio.EDGE](#edge)} Pin edge. **Default:** `gpio.EDGE.NONE`
* `callback` {Function}
  * `error` {Error|null}
  * `gpioPin` {Object} An instance of GPIOPin.
* Returns: {Object} An instance of GPIOPin.

Get GPIOPin object with configuration asynchronously.

The `callback` function will be called after
opening is completed. The `error` argument is an
`Error` object on failure or `null` otherwise.

**Example**

```js
var gpio = require('gpio');

var gpio10 = gpio.open({
  pin: 10,
  direction: gpio.DIRECTION.OUT,
  mode: gpio.MODE.PUSHPULL,
  edge: gpio.EDGE.RISING
}, function(err, pin) {
  if (err) {
    throw err;
  }
});
```

### gpio.openSync(configuration)
* `configuration` {Object} Configuration for open GPIOPin.
  * `pin` {number} Pin number. Mandatory field.
  * `direction` {[gpio.DIRECTION](#direction)} Pin direction. **Default:** `gpio.DIRECTION.OUT`
  * `mode` {[gpio.MODE](#mode)} Pin mode. **Default:** `gpio.MODE.NONE`
  * `edge` {[gpio.EDGE](#edge)} Pin edge. **Default:** `gpio.EDGE.NONE`
* Returns: {Object} An instance of GPIOPin.

Get GPIOPin object with configuration synchronously.

**Example**

```js
var gpio = require('gpio');

var gpio10 = gpio.openSync({
  pin: 10,
  direction: gpio.DIRECTION.IN,
  mode: gpio.MODE.PULLUP
});
```


## Class: GPIOPin

This class represents an opened and configured GPIO pin.
It allows getting and setting the status of the pin.

### gpiopin.setDirectionSync(direction)
  * `direction` {[gpio.DIRECTION](#direction)} Pin direction.

Set the direction of a GPIO pin.

**Example**

```js
gpio10.setDirectionSync(gpio.DIRECTION.OUT);
gpio10.writeSync(1);

gpio10.setDirectionSync(gpio.DIRECTION.IN);
var value = gpio10.readSync();
```

### gpiopin.write(value[, callback])
* `value` {number|boolean}
* `callback` {Function}
  * `error` {Error|null}

Asynchronously writes out a boolean `value` to a GPIO pin
(a number `value` is converted to boolean first).

The optional `callback` function will be called
after the write is completed. The `error` argument
is an `Error` object on failure or `null` otherwise.

**Example**

```js
gpio10.write(1, function(err) {
  if (err) {
    throw err;
  }
});
```


### gpiopin.writeSync(value)
* `value` {number|boolean}

Writes out a boolean `value` to a GPIO pin synchronously
(a number `value` is converted to boolean first).

**Example**

```js
gpio10.writeSync(1);
```


### gpiopin.read([callback])
* `callback` {Function}
  * `error` {Error|null}
  * `value` {boolean}

Asynchronously reads a boolean value from a GPIO pin.

The optional `callback` function will be called
after the read is completed. The `error` argument
is an `Error` object on failure or `null` otherwise.
The `value` argument contains the boolean value
of the pin.

**Example**

```js
gpio10.read(function(err, value) {
  if (err) {
    throw err;
  }
  console.log('value:', value);
});
```


### gpiopin.readSync()
* Returns: {boolean}

Returns the boolean value of a GPIO pin.

**Example**

```js
console.log('value:', gpio10.readSync());
```


### gpiopin.close([callback])
* `callback` {Function}
  * `error` {Error|null}

Asynchronously closes a GPIO pin.

The optional `callback` function will be called
after the close is completed. The `error` argument
is an `Error` object on failure or `null` otherwise.

**Example**

```js
gpio10.close(function(err) {
  if (err) {
    throw err;
  }

  // prints: gpio pin is closed
  console.log('gpio pin is closed');
});
```


### gpiopin.closeSync()

Synchronously closes a GPIO pin.

**Example**

```js
gpio10.closeSync();

// prints: gpio pin is closed
console.log('gpio pin is closed');
```
