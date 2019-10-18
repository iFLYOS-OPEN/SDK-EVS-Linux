### Platform Support

The following shows fs module APIs available for each platform.

|  | Linux<br/>(Ubuntu) | Tizen<br/>(Raspberry Pi) | Raspbian<br/>(Raspberry Pi) | NuttX<br/>(STM32F4-Discovery) | TizenRT<br/>(Artik053) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| fs.close | O | O | O | O | O |
| fs.closeSync | O | O | O | O | O |
| fs.createReadStream | O | O | O | O | O |
| fs.createWriteStream | O | O | O | O | O |
| fs.exists | O | O | O | O | O |
| fs.existsSync | O | O | O | O | O |
| fs.fstat | O | O | O | X | X |
| fs.fstatSync | O | O | O | X | X |
| fs.mkdir | O | O | O | O | O |
| fs.mkdirSync | O | O | O | O | O |
| fs.open | O | O | O | O | O |
| fs.openSync | O | O | O | O | O |
| fs.read | O | O | O | O | O |
| fs.readSync | O | O | O | O | O |
| fs.readdir | O | O | O | O | O |
| fs.readdirSync | O | O | O | O | O |
| fs.readFile | O | O | O | O | O |
| fs.readFileSync | O | O | O | O | O |
| fs.rename | O | O | O | O | O |
| fs.renameSync | O | O | O | O | O |
| fs.rmdir | O | O | O | O | O |
| fs.rmdirSync | O | O | O | O | O |
| fs.stat | O | O | O | O | O |
| fs.statSync | O | O | O | O | O |
| fs.unlink | O | O | O | O | O |
| fs.unlinkSync | O | O | O | O | O |
| fs.write | O | O | O | O | O |
| fs.writeSync | O | O | O | O | O |
| fs.writeFile | O | O | O | O | O |
| fs.writeFileSync | O | O | O | O | O |

※ On NuttX path should be passed with a form of **absolute path**.


# File System

## Class: fs.Stats

fs.Stats class is an object returned from `fs.stat()`,`fs.fstat()` and their synchronous counterparts.


### stats.isDirectory()
* Returns: {boolean}

Returns true if stated file is a directory.


### stats.isFile()
* Returns: {boolean}

Returns true if stated file is a file.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

fs.stat('test.txt', function(err, stat) {
  if (err) {
    throw err;
  }
  assert.equal(stat.isFile(), true);
  assert.equal(stat.isDirectory(), false);
});
```


### fs.close(fd, callback)
* `fd` {integer} File descriptor.
* `callback` {Function}
  * `err` {Error|null}

Closes the file of `fd` asynchronously.

**Example**

```js
var fs = require('fs');

fs.open('test.txt', 'r', function(err, fd) {
  if (err) {
    throw err;
  }
  // do something
  fs.close(fd, function(err) {
    if (err) {
      throw err;
    }
  });
});
```


### fs.closeSync(fd)
* `fd` {integer} File descriptor.

Closes the file of `fd` synchronously.

**Example**

```js
var fs = require('fs');

var fd = fs.openSync('test.txt', 'r');
// do something
fs.closeSync(fd);
```


## Class: fs.ReadStream

A successful call to `fs.createReadStream()` will return a new `fs.ReadStream`
object.

`fs.ReadStream` inherits from `stream.Readable`.


### Event: 'open'
* `fd` {integer} File descriptor used by the `fs.ReadStream`.

Emitted when the `fs.ReadStream`'s file descriptor has been opened.


### Event: 'ready'

Emitted when the `fs.ReadStream` is ready to be used. Emitted immediately
after `open`.


### Event: 'data'
* `chunk` {Buffer|string}

Inherited from `stream.Readable`. Emitted when the stream passes the ownership
of the data to a consumer. Only streams in flowing mode emit this event.

A stream can be switched to flowing mode by calling the readable.resume()
function or by adding a 'data' event handler.


### Event: 'close'

Emitted when the `fs.ReadStream`'s file descriptor has been closed.


### ReadStream.bytesRead
* {integer}

Number of bytes that have been read so far.


### ReadStream.path
* {string}

The path to the file of the `fs.ReadStream`.


### fs.createReadStream(path[, options])
* `path` {string} File path to open for reading.
* `options` {Object}
  * `flags` {string} Flags to open file with. **Default:** `'r'`
  * `encoding` {string} **Default:** `null`
  * `fd` {integer} File descriptor to be used. **Default:** `null`
  * `mode` {integer} Permission mode. **Default:** `0666`
  * `autoClose` {boolean} Should the file be closed automatically. **Default:** `true`
  * `bufferSize` {integer} Size of buffer in bytes. **Default:** `4096`
* Returns: `fs.ReadStream`

If `fd` is specified, `path` will be ignored and the specified file
descriptor will be used instead.

If `autoClose` is `false`, the file will not be closed automatically,
even if there is an error, it will be the application's responsibility.
If it is `true` (as by default), the file will be closed automatically
when end of file is reached or the stream ends.

**Example**

```js
var fs = require('fs');

var rStream = fs.createReadStream('example.txt');
rStream.on('data', function(data) {
  console.log(data.toString());
});
```

`fs.ReadStream` inherits from `stream.Readable`, so it is possible to pipe
it into any `stream.Writable`.

**Example**

```js
var fs = require('fs');

var readableFileStream = fs.createReadStream('in.txt');
var writableFileStream = fs.createWriteStream('out.txt');

// The content of in.txt will be copied to out.txt
readableFileStream.pipe(writableFileStream);
```


## Class: fs.WriteStream

A successful call to `fs.createWriteStream()` will return a new `fs.WriteStream`
object.

`fs.WriteStream` inherits from `stream.Writable`.


### Event: 'open'
* `fd` {integer} File descriptor used by the `fs.WriteStream`.

Emitted when the `fs.WriteStream`'s file descriptor has been opened.


### Event: 'ready'

Emitted when the `fs.WriteStream` is ready to be used. Emitted immediately
after `open`.


### Event: 'close'

Emitted when the `fs.WriteStream`'s file descriptor has been closed.


### WriteStream.bytesWritten

The number of bytes written so far. Does not include data that is still queued
for writing.


### WriteStream.path

The path to the file of the `fs.WriteStream`.


### fs.createWriteStream
* `path` {string} File path to be opened for writing.
* `options` {Object}
  * `flags` {string} Flags to open the file with. **Default:** `'w'`
  * `fd` {integer} File descriptor to be used. **Default:** `null`
  * `mode` {integer} Permission mode. **Default:** `0666`
  * `autoClose` {boolean} Should the file be closed automatically. **Default:** `true`
* Returns `fs.WriteStream`

Works similarly to `fs.createReadStream()`, but returns an `fs.WriteStream`.

If `fd` is specified, `path` will be ignored and the specified file
descriptor will be used instead.

If `autoClose` is `false`, the file will not be closed automatically,
even if there is an error, it will be the application's responsibility.
If it is `true` (as by default), the file will be closed automatically
when end of file is reached or the stream ends.

**Example**

```js
var fs = require('fs');

var wStream = fs.createWriteStream('example.txt');

wStream.on('ready', function() {
  wStream.write('test data');
  // 'test data' will be written into example.txt
});
```


### fs.exists(path, callback)
* `path` {string} File path to be checked.
* `callback` {Function}
  * `exists` {boolean}

Checks the file specified by `path` exists asynchronously.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

fs.exists('test.txt', function(exists) {
  assert.equal(exists, true);
});
```


### fs.existsSync(path)
* `path` {string} File path to be checked.
* Returns: {boolean} True if the file exists, otherwise false.

Checks the file specified by `path` exists synchronously.

```js
var assert = require('assert');
var fs = require('fs');

var result = fs.existsSync('test.txt');
assert.equal(result, true);
```

### fs.fstat(fd, callback)
* `fd` {integer} File descriptor to be stated.
* `callback` {Function}
  * `err` {Error|null}
  * `stat` {Object} An instance of `fs.Stats`.

Get information about a file what specified by `fd` into `stat` asynchronously.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

fs.open('test.txt', 'r', function(err, fd) {
  if (err) {
    throw err;
  }
  fs.fstat(fd, function(err, stat) {
    if (err) {
      throw err;
    }
    assert.equal(stat.isFile(), true);
    assert.equal(stat.isDirectory(), false);
  });
});
```

### fs.fstatSync(fd)
* `fd` {integer} - File descriptor to be stated.
* Returns: {Object} An instance of `fs.Stats`.

Get information about a file what specified by `fd` synchronously.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

fs.open('test.txt', 'r', function(err, fd) {
  if (err) {
    throw err;
  }
  var stat = fs.fstatSync(fd);
  assert.equal(stat.isFile(), true);
  assert.equal(stat.isDirectory(), false);
});
```


### fs.mkdir(path[, mode], callback)
* `path` {string} Path of the directory to be created.
* `mode` {string|number} Permission mode. **Default:** `0777`
* `callback` {Function}
  * `err` {Error|null}

Creates the directory specified by `path` asynchronously.

**Example**

```js
var fs = require('fs');

fs.mkdir('testdir', function(err) {
  if (err) {
    throw err;
  }
});
```


### fs.mkdirSync(path[, mode])
* `path` {string} Path of the directory to be created.
* `mode` {string|number} Permission mode. **Default:** `0777`

Creates the directory specified by `path` synchronously.

**Example**

```js
var fs = require('fs');

fs.mkdirSync('testdir');
```


### fs.open(path, flags[, mode], callback)
* `path` {string} File path to be opened.
* `flags` {string} Open flags.
* `mode` {string|number} Permission mode. **Default:** `0666`
* `callback` {Function}
  * `err` {Error|null}
  * `fd` {number}

Opens file asynchronously.

`flags` can be:
  * `r` Opens file for reading. Throws an exception if the file does not exist.
  * `rs` or `sr` Opens file for reading in synchronous mode. Throws an exception if the file does not exist.
  * `r+` Opens file for reading and writing. Throws an exception if the file does not exist.
  * `rs+` or `sr+` Opens file for reading and writing in synchronous mode. Throws an exception if the file does not exist.
  * `w` Opens file for writing. The file is overwritten if it exists.
  * `wx` or `xw` Opens file for writing. Throws an exception if it exists.
  * `w+` Opens file for reading and writing. The file is overwritten if it exists.
  * `wx+` or `xw+` Opens file for reading and writing. Throws an exception if it exists.
  * `a` Opens file for appending. The file is created if it does not exist.
  * `ax` or `xa` Opens file for appending. Throws an exception if it exists.
  * `a+` Opens file for reading and appending. The file is created if it does not exist.
  * `ax+` or `xa+` Opens file for reading and appending. Throws an exception if it exists.

**Example**

```js
var fs = require('fs');

fs.open('test.txt', 'r', 755, function(err, fd) {
  if (err) {
    throw err;
  }
  // do something
});
```


### fs.openSync(path, flags[, mode])
* `path` {string} File path to be opened.
* `flags` {string} Open flags.
* `mode` {string|number} Permission mode. **Default:** `0666`
* Returns: {number} File descriptor.

Opens file synchronously.

For available options of the `flags` see [fs.open()](#class-method-fsopenpath-flags-mode-callback).

**Example**

```js
var fs = require('fs');

var fd = fs.openSync('test.txt', 'r', 755);
// do something
```


### fs.read(fd, buffer, offset, length, position, callback)
* `fd` {integer} File descriptor.
* `buffer` {Buffer} Buffer that the data will be written to.
* `offset` {number} Offset of the buffer where to start writing.
* `length` {number} Number of bytes to read.
* `position` {number} Specifying where to start read data from the file, if `null` or `undefined`, read from current position.
* `callback` {Function}
  * `err` {Error|null}
  * `bytesRead` {number}
  * `buffer` {Buffer}

Reads data from the file specified by `fd` asynchronously.

**Example**

```js
var fs = require('fs');

fs.open('test.txt', 'r', 755, function(err, fd) {
  if (err) {
    throw err;
  }
  var buffer = new Buffer(64);
  fs.read(fd, buffer, 0, buffer.length, 0, function(err, bytesRead, buffer) {
    if (err) {
      throw err;
    }
  });
});
```


### fs.readSync(fd, buffer, offset, length, position)
* `fd` {integer} File descriptor.
* `buffer` {Buffer} Buffer that the data will be written to.
* `offset` {number} Offset of the buffer where to start writing.
* `length` {number} Number of bytes to read.
* `position` {number} Specifying where to start read data from the file, if `null` or `undefined`, read from current position.
* Returns: {number} Number of read bytes.

Reads data from the file specified by `fd` synchronously.

**Example**

```js
var fs = require('fs');

var buffer = new Buffer(16);
var fd = fs.openSync('test.txt', 'r');
var bytesRead = fs.readSync(fd, buffer, 0, buffer.length, 0);
```


### fs.readdir(path, callback)
* `path` {string} Directory path to be checked.
* `callback` {Function}
  * `err` {Error|null}
  * `files` {Object}

Reads the contents of the directory specified by `path` asynchronously, `.` and `..` are excluded from `files`.

**Example**

```js
var fs = require('fs');

fs.readdir('testdir', function(err, items) {
  if (err) {
    throw err;
  }

  // prints: file1,file2,... from 'testdir'
  console.log(items);
});
```


### fs.readdirSync(path)
* `path` {string} Directory path to be checked.
* Returns: {Object} Array of filenames.

Reads the contents of the directory specified by `path` synchronously, `.` and `..` are excluded from filenames.

**Example**

```js
var fs = require('fs');

var items = fs.readdirSync('testdir');

// prints: file1,file2,... from 'testdir'
console.log(items);
```


### fs.readFile(path, callback)
* `path` {string} File path to be opened.
* `callback` {Function}
  * `err` {Error|null}
  * `data` {Buffer}

Reads entire file asynchronously into `data`.

**Example**

```js
var fs = require('fs');

fs.readFile('test.txt', function(err, data) {
  if (err) {
    throw err;
  }

  // prints: the content of 'test.txt'
  console.log(data);
});
```


### fs.readFileSync(path)
* `path` {string} File path to be opened.
* Returns: {Object} Contents of the file.

Reads entire file synchronously.

**Example**

```js
var fs = require('fs');

var data = fs.readFileSync('test.txt');
```


### fs.rename(oldPath, newPath, callback)
* `oldPath` {string} Old file path.
* `newPath` {string} New file path.
* `callback` {Function}
  * `err` {Error|null}

Renames `oldPath` to `newPath` asynchronously.

**Example**

```js
var fs = require('fs');

fs.rename('test.txt', 'test.txt.async', function(err) {
  if (err) {
    throw err;
  }
});
```


### fs.renameSync(oldPath, newPath)
* `oldPath` {string} Old file path.
* `newPath` {string} New file path.

Renames `oldPath` to `newPath` synchronously.

**Example**

```js
var fs = require('fs');

fs.renameSync('test.txt', 'test.txt.sync');
```


### fs.rmdir(path, callback)
* `path` {string} Directory path to be removed.
* `callback` {Function}
  * `err` {Error|null}

Removes the directory specified by `path` asynchronously.

**Example**

```js
var fs = require('fs');

fs.rmdir('testdir', function() {
  // do something
});
```


### fs.rmdirSync(path)
* `path` {string} Directory path to be removed.

Removes the directory specified by `path` synchronously.

```js
var fs = require('fs');

fs.rmdirSync('testdir');
```


### fs.stat(path, callback)
* `path` {string} File path to be stated.
* `callback` {Function}
  * `err` {Error|null}
  * `stat` {Object}

Get information about a file into `stat` asynchronously.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

fs.stat('test.txt', function(err, stat) {
  if (err) {
    throw err;
  }
  assert.equal(stat.isFile(), true);
  assert.equal(stat.isDirectory(), false);
});
```


### fs.statSync(path)
* `path` {string} File path to be stated.
* Returns: {Object} An instance of `fs.Stats`.

Get information about a file synchronously.

**Example**

```js
var assert = require('assert');
var fs = require('fs');

var stat = fs.statSync('test.txt');
assert.equal(stat.isFile(), true);
assert.equal(stat.isDirectory(), false);
```


### fs.unlink(path, callback)
* `path` {string} File path to be removed.
* `callback` {Function}
  * `err` {Error|null}

Removes the file specified by `path` asynchronously.

**Example**

```js
var fs = require('fs');

fs.unlink('test.txt', function(err) {
  if (err) {
    throw err;
  }
});
```


### fs.unlinkSync(path)
* `path` {string} File path to be removed.

Removes the file specified by `path` synchronously.

**Example**

```js
var fs = require('fs');

fs.unlinkSync('test.txt');
```


### fs.write(fd, buffer, offset, length[, position], callback)
* `fd` {integer} File descriptor.
* `buffer` {Buffer} Buffer that the data will be written from.
* `offset` {number} Offset of the buffer where from start reading.
* `length` {number} Number of bytes to write.
* `position` {number} Specifying where to start write data to the file, if `null` or `undefined`, write at the current position.
* `callback` {Function}
  * `err` {Error|null}
  * `bytesWrite` {integer}
  * `buffer` {Object}

Writes `buffer` to the file specified by `fd` asynchronously.

**Example**

```js
var fs = require('fs');

var file = 'test.txt'
var data = new Buffer('IoT.js');

fs.open(file, 'w', function(err, fd) {
  if (err) {
    throw err;
  }

  fs.write(fd, data, 0, data.length, function(err, bytesWrite, buffer) {
    if (err) {
      throw err;
    }

    // prints: 6
    console.log(bytesWrite);

    // prints: IoT.js
    console.log(buffer);
  });
});
```

### fs.writeSync(fd, buffer, offset, length[, position])
* `fd` {integer} File descriptor.
* `buffer` {Buffer} Buffer that the data will be written from.
* `offset` {number} Offset of the buffer where from start reading.
* `length` {number} Number of bytes to write.
* `position` {number} Specifying where to start write data to the file, if `null` or `undefined`, write at the current position.
* Returns: {number} Number of bytes written.

Writes buffer to the file specified by `fd` synchronously.

```js
var fs = require('fs');

var file = 'test.txt'
var data = new Buffer('IoT.js');

var fd = fs.openSync(file, 'w');
var bytes = fs.writeSync(fd, data, 0, data.length);

//prints: 6
console.log(bytes);
```


### fs.writeFile(path, data, callback)
* `path` {string} File path that the `data` will be written.
* `data` {string|Buffer} String or buffer that contains data.
* `callback` {Function}
  * `err` {Error|null}

Writes entire `data` to the file specified by `path` asynchronously.

**Example**

```js
var fs = require('fs');

fs.writeFile('test.txt', 'IoT.js', function(err) {
  if (err) {
    throw err;
  }
});
```


### fs.writeFileSync(path, data)
* `path` {string} File path that the `data` will be written.
* `data` {string|Buffer} String or buffer that contains data.

Writes entire `data` to the file specified by `path` synchronously.

**Example**

```js
var fs = require('fs');

fs.writeFileSync('test.txt', 'IoT.js');
```
