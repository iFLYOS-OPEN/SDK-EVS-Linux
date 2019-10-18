/* Copyright 2015-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


var fs = exports;
var constants = require('constants');
var util = require('util');
var fsBuiltin = native;

fs.exists = function(path, callback) {
  if (!(util.isString(path)) && !(util.isBuffer(path))) {
    throw new TypeError('Path should be a string or a buffer');
  }
  if (!path || !path.length) {
    process.nextTick(function() {
      if (callback) callback(false);
    });
    return;
  }

  var cb = function(err/* , stat */) {
    if (callback) callback(err ? false : true);
  };

  fsBuiltin.stat(checkArgString(path, 'path'),
                 checkArgFunction(cb, 'callback'));
};


fs.existsSync = function(path) {
  if (!path || !path.length) {
    return false;
  }

  try {
    fsBuiltin.stat(checkArgString(path, 'path'));
    return true;
  } catch (e) {
    return false;
  }
};


fs.stat = function(path, callback) {
  fsBuiltin.stat(checkArgString(path, 'path'),
                 checkArgFunction(callback, 'callback'));
};


fs.statSync = function(path) {
  return fsBuiltin.stat(checkArgString(path, 'path'));
};


fs.fstat = function(fd, callback) {
  fsBuiltin.fstat(checkArgNumber(fd, 'fd'),
                  checkArgFunction(callback, 'callback'));
};


fs.fstatSync = function(fd) {
  return fsBuiltin.fstat(checkArgNumber(fd, 'fd'));
};


fs.close = function(fd, callback) {
  fsBuiltin.close(checkArgNumber(fd, 'fd'),
                  checkArgFunction(callback, 'callback'));
};


fs.closeSync = function(fd) {
  fsBuiltin.close(checkArgNumber(fd, 'fd'));
};


fs.open = function(path, flags, mode/* , callback */) {
  fsBuiltin.open(checkArgString(path, 'path'),
                 convertFlags(flags),
                 convertMode(mode, 438),
                 checkArgFunction(arguments[arguments.length - 1]), 'callback');
};


fs.openSync = function(path, flags, mode) {
  return fsBuiltin.open(checkArgString(path, 'path'),
                        convertFlags(flags),
                        convertMode(mode, 438));
};


fs.read = function(fd, buffer, offset, length, position, callback) {
  if (util.isNullOrUndefined(position)) {
    position = -1; // Read from the current position.
  }

  callback = checkArgFunction(callback, 'callback');

  var cb = function(err, bytesRead) {
    callback(err, bytesRead || 0, buffer);
  };

  return fsBuiltin.read(checkArgNumber(fd, 'fd'),
                        checkArgBuffer(buffer, 'buffer'),
                        checkArgNumber(offset, 'offset'),
                        checkArgNumber(length, 'length'),
                        checkArgNumber(position, 'position'),
                        cb);
};


fs.readSync = function(fd, buffer, offset, length, position) {
  if (util.isNullOrUndefined(position)) {
    position = -1; // Read from the current position.
  }

  return fsBuiltin.read(checkArgNumber(fd, 'fd'),
                        checkArgBuffer(buffer, 'buffer'),
                        checkArgNumber(offset, 'offset'),
                        checkArgNumber(length, 'length'),
                        checkArgNumber(position, 'position'));
};


fs.write = function(fd, buffer, offset, length, position, callback) {
  if (util.isFunction(position)) {
    callback = position;
    position = -1; // write at current position.
  } else if (util.isNullOrUndefined(position)) {
    position = -1; // write at current position.
  }

  callback = checkArgFunction(callback, 'callback');

  var cb = function(err, written) {
    callback(err, written, buffer);
  };

  return fsBuiltin.write(checkArgNumber(fd, 'fd'),
                         checkArgBuffer(buffer, 'buffer'),
                         checkArgNumber(offset, 'offset'),
                         checkArgNumber(length, 'length'),
                         checkArgNumber(position, 'position'),
                         cb);
};


fs.writeSync = function(fd, buffer, offset, length, position) {
  if (util.isNullOrUndefined(position)) {
    position = -1; // write at current position.
  }

  return fsBuiltin.write(checkArgNumber(fd, 'fd'),
                         checkArgBuffer(buffer, 'buffer'),
                         checkArgNumber(offset, 'offset'),
                         checkArgNumber(length, 'length'),
                         checkArgNumber(position, 'position'));
};


fs.readFile = function(path, callback) {
  checkArgString(path);
  checkArgFunction(callback);

  var fd;
  var buffers;

  fs.open(path, 'r', function(err, _fd) {
    if (err) {
      return callback(err);
    }

    fd = _fd;
    buffers = [];

    // start read
    read();
  });

  var read = function() {
    // Read segment of data.
    var buffer = new Buffer(1023);
    fs.read(fd, buffer, 0, 1023, -1, afterRead);
  };

  var afterRead = function(err, bytesRead, buffer) {
    if (err) {
      fs.close(fd, function(err) {
        return callback(err);
      });
    }

    if (bytesRead === 0) {
      // End of file.
      close();
    } else {
      // continue reading.
      buffers.push(buffer.slice(0, bytesRead));
      read();
    }
  };

  var close = function() {
    fs.close(fd, function(err) {
      return callback(err, Buffer.concat(buffers));
    });
  };
};


fs.readFileSync = function(path) {
  checkArgString(path);

  var fd = fs.openSync(path, 'r', 438);
  var buffers = [];

  while (true) {
    try {
      var buffer = new Buffer(1023);
      var bytesRead = fs.readSync(fd, buffer, 0, 1023);
      if (bytesRead) {
        buffers.push(buffer.slice(0, bytesRead));
      } else {
        break;
      }
    } catch (e) {
      break;
    }
  }
  fs.closeSync(fd);

  return Buffer.concat(buffers);
};


fs.writeFile = function(path, data, callback) {
  checkArgString(path);
  checkArgFunction(callback);

  var fd;
  var len;
  var bytesWritten;
  var buffer = ensureBuffer(data);

  fs.open(path, 'w', function(err, _fd) {
    if (err) {
      return callback(err);
    }

    fd = _fd;
    len = buffer.length;
    bytesWritten = 0;

    write();
  });

  var write = function() {
    var tryN = (len - bytesWritten) >= 1024 ? 1023 : (len - bytesWritten);
    fs.write(fd, buffer, bytesWritten, tryN, bytesWritten, afterWrite);
  };

  var afterWrite = function(err, n) {
    if (err) {
      fs.close(fd, function(err) {
        return callback(err);
      });
    }

    if (n <= 0 || bytesWritten + n == len) {
      // End of buffer
      fs.close(fd, function(err) {
        callback(err);
      });
    } else {
      // continue writing
      bytesWritten += n;
      write();
    }
  };
};


fs.writeFileSync = function(path, data) {
  checkArgString(path);

  var buffer = ensureBuffer(data);
  var fd = fs.openSync(path, 'w');
  var len = buffer.length;
  var bytesWritten = 0;

  while (true) {
    try {
      var tryN = (len - bytesWritten) >= 1024 ? 1023 : (len - bytesWritten);
      var n = fs.writeSync(fd, buffer, bytesWritten, tryN, bytesWritten);
      bytesWritten += n;
      if (bytesWritten == len) {
        break;
      }
    } catch (e) {
      break;
    }
  }
  fs.closeSync(fd);
  return bytesWritten;
};


fs.mkdir = function(path, mode, callback) {
  if (util.isFunction(mode)) callback = mode;
  checkArgString(path, 'path');
  checkArgFunction(callback, 'callback');
  fsBuiltin.mkdir(path, convertMode(mode, 511), callback);
};


fs.mkdirSync = function(path, mode) {
  return fsBuiltin.mkdir(checkArgString(path, 'path'),
                         convertMode(mode, 511));
};


fs.rmdir = function(path, callback) {
  checkArgString(path, 'path');
  checkArgFunction(callback, 'callback');
  fsBuiltin.rmdir(path, callback);
};


fs.rmdirSync = function(path) {
  return fsBuiltin.rmdir(checkArgString(path, 'path'));
};


fs.unlink = function(path, callback) {
  checkArgString(path);
  checkArgFunction(callback);
  fsBuiltin.unlink(path, callback);
};


fs.unlinkSync = function(path) {
  return fsBuiltin.unlink(checkArgString(path, 'path'));
};


fs.rename = function(oldPath, newPath, callback) {
  checkArgString(oldPath);
  checkArgString(newPath);
  checkArgFunction(callback);
  fsBuiltin.rename(oldPath, newPath, callback);
};


fs.renameSync = function(oldPath, newPath) {
  checkArgString(oldPath);
  checkArgString(newPath);
  fsBuiltin.rename(oldPath, newPath);
};


fs.readdir = function(path, callback) {
  checkArgString(path);
  checkArgFunction(callback);
  fsBuiltin.readdir(path, callback);
};


fs.readdirSync = function(path) {
  return fsBuiltin.readdir(checkArgString(path, 'path'));
};


try {
  var stream = require('stream');
  var Readable = stream.Readable;
  var Writable = stream.Writable;


  var ReadStream = function(path, options) {
    if (!(this instanceof ReadStream)) {
      return new ReadStream(path, options);
    }

    options = options || {};

    Readable.call(this, {defaultEncoding: options.encoding || null});

    this.bytesRead = 0;
    this.path = path;
    this._autoClose = util.isNullOrUndefined(options.autoClose) ||
                                             options.autoClose;
    this._fd = options.fd;
    this._buff = new Buffer(options.bufferSize || 4096);

    var self = this;
    if (util.isNullOrUndefined(this._fd)) {
      fs.open(this.path, options.flags || 'r', options.mode || 438,
              function(err, _fd) {
        if (err) {
          throw err;
        }
        self._fd = _fd;
        self.emit('open', self._fd);
        self.doRead();
      });
    }

    this.once('open', function(/* _fd */) {
      this.emit('ready');
    });

    if (this._autoClose) {
      this.on('end', function() {
        closeFile(self);
      });
    }
  };


  util.inherits(ReadStream, Readable);


  ReadStream.prototype.doRead = function() {
    var self = this;
    fs.read(this._fd, this._buff, 0, this._buff.length, null,
            function(err, bytes_read/* , buffer*/) {
      if (err) {
        if (self._autoClose) {
          closeFile(self);
        }
        throw err;
      }

      self.bytesRead += bytes_read;
      if (bytes_read === 0) {
        // Reached end of file.
        // null must be pushed so the 'end' event will be emitted.
        self.push(null);
      } else {
        self.push(bytes_read == self._buff.length ?
                  self._buff : self._buff.slice(0, bytes_read));
        self.doRead();
      }
    });
  };


  fs.createReadStream = function(path, options) {
    return new ReadStream(path, options);
  };


  var WriteStream = function(path, options) {
    if (!(this instanceof WriteStream)) {
      return new WriteStream(path, options);
    }

    options = options || {};

    Writable.call(this);

    this._fd = options._fd;
    this._autoClose = util.isNullOrUndefined(options.autoClose) ||
                                             options.autoClose;
    this.bytesWritten = 0;

    var self = this;
    if (!this._fd) {
      fs.open(path, options.flags || 'w', options.mode || 438,
              function(err, _fd) {
        if (err) {
          throw err;
        }
        self._fd = _fd;
        self.emit('open', self._fd);
      });
    }

    this.once('open', function(/* _fd */) {
      self.emit('ready');
    });

    if (this._autoClose) {
      this.on('finish', function() {
        closeFile(self);
      });
    }

    this._readyToWrite();
  };


  util.inherits(WriteStream, Writable);


  WriteStream.prototype._write = function(chunk, callback, onwrite) {
    var self = this;
    fs.write(this._fd, chunk, 0, chunk.length,
             function(err, bytes_written/* , buffer */) {
      if (err) {
        if (self._autoClose) {
          closeFile(self);
        }
        throw err;
      }
      this.bytesWritten += bytes_written;

      if (callback) {
        callback();
      }
      onwrite();
    });
  };


  fs.createWriteStream = function(path, options) {
    return new WriteStream(path, options);
  };


  var closeFile = function(stream) {
    fs.close(stream._fd, function(err) {
      if (err) {
        throw err;
      }
      stream.emit('close');
    });
  };
} catch (e) {
}


function convertFlags(flag) {
  var O_APPEND = constants.O_APPEND;
  var O_CREAT = constants.O_CREAT;
  var O_EXCL = constants.O_EXCL;
  var O_RDONLY = constants.O_RDONLY;
  var O_RDWR = constants.O_RDWR;
  var O_SYNC = constants.O_SYNC;
  var O_TRUNC = constants.O_TRUNC;
  var O_WRONLY = constants.O_WRONLY;

  if (util.isString(flag)) {
    switch (flag) {
      case 'r': return O_RDONLY;
      case 'rs':
      case 'sr': return O_RDONLY | O_SYNC;

      case 'r+': return O_RDWR;
      case 'rs+':
      case 'sr+': return O_RDWR | O_SYNC;

      case 'w': return O_TRUNC | O_CREAT | O_WRONLY;
      case 'wx':
      case 'xw': return O_TRUNC | O_CREAT | O_WRONLY | O_EXCL;

      case 'w+': return O_TRUNC | O_CREAT | O_RDWR;
      case 'wx+':
      case 'xw+': return O_TRUNC | O_CREAT | O_RDWR | O_EXCL;

      case 'a': return O_APPEND | O_CREAT | O_WRONLY;
      case 'ax':
      case 'xa': return O_APPEND | O_CREAT | O_WRONLY | O_EXCL;

      case 'a+': return O_APPEND | O_CREAT | O_RDWR;
      case 'ax+':
      case 'xa+': return O_APPEND | O_CREAT | O_RDWR | O_EXCL;
    }
  }
  throw new TypeError('Bad argument: flags');
}


function convertMode(mode, def) {
  if (util.isNumber(mode)) {
    return mode;
  } else if (util.isString(mode)) {
    return parseInt(mode, 8);
  } else if (def) {
    return convertMode(def);
  }
  return undefined;
}


function ensureBuffer(data) {
  if (util.isBuffer(data)) {
    return data;
  }

  return new Buffer(data + ''); // coert to string and make it a buffer
}


function checkArgType(value, name, checkFunc) {
  if (checkFunc(value)) {
    return value;
  } else {
    throw new TypeError('Bad arguments: ' + name);
  }
}


function checkArgBuffer(value, name) {
  return checkArgType(value, name, util.isBuffer);
}


function checkArgNumber(value, name) {
  return checkArgType(value, name, util.isNumber);
}


function checkArgString(value, name) {
  return checkArgType(value, name, util.isString);
}


function checkArgFunction(value, name) {
  return checkArgType(value, name, util.isFunction);
}
