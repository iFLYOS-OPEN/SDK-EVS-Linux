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

var Buffer = require('buffer');


function isNull(arg) {
  return arg === null;
}


function isUndefined(arg) {
  return arg === undefined;
}


function isNullOrUndefined(arg) {
  return arg === null || arg === undefined;
}


function isNumber(arg) {
  return typeof arg === 'number';
}

function isFinite(arg) {
  return (arg == 0) || (arg != arg / 2);
}

function isBoolean(arg) {
  return typeof arg === 'boolean';
}


function isString(arg) {
  return typeof arg === 'string';
}


function isObject(arg) {
  return typeof arg === 'object' && arg != null;
}


function isFunction(arg) {
  return typeof arg === 'function';
}


function inherits(ctor, superCtor) {
  ctor.prototype = Object.create(superCtor.prototype, {
    constructor: {
      value: ctor,
      enumerable: false,
      writable: true,
      configurable: true,
    },
  });
}


function mixin(target) {
  if (isNullOrUndefined(target)) {
    throw new TypeError('target cannot be null or undefined');
  }

  for (var i = 1; i < arguments.length; ++i) {
    var source = arguments[i];
    if (!isNullOrUndefined(source)) {
      for (var prop in source) {
        if (source.hasOwnProperty(prop)) {
          target[prop] = source[prop];
        }
      }
    }
  }

  return target;
}

function formatWithTag() {
  var arrs = [new Date().toISOString(), this.tag + ':'];
  var i;
  for (i = 0; i < arguments.length; ++i) {
    arrs.push(formatValue(arguments[i]));
  }
  return arrs.join(' ')
}

function format(s) {
  var i;
  if (!isString(s)) {
    var arrs = [];
    for (i = 0; i < arguments.length; ++i) {
      arrs.push(formatValue(arguments[i]));
    }
    return arrs.join(' ');
  }

  i = 1;
  var args = arguments;
  var arg_string;
  var str = '';
  var start = 0;
  var end = 0;

  while (end < s.length) {
    if (s.charAt(end) !== '%') {
      end++;
      continue;
    }

    str += s.slice(start, end);

    switch (s.charAt(end + 1)) {
      case 's':
        arg_string = String(args[i]);
        break;
      case 'd':
        arg_string = Number(args[i]);
        break;
      case 'j':
        try {
          arg_string = JSON.stringify(args[i]);
        } catch (_) {
          arg_string = '[Circular]';
        }
        break;
      case '%':
        str += '%';
        start = end = end + 2;
        continue;
      default:
        str = str + '%' + s.charAt(end + 1);
        start = end = end + 2;
        continue;
    }

    if (i >= args.length) {
      str = str + '%' + s.charAt(end + 1);
    } else {
      i++;
      str += arg_string;
    }

    start = end = end + 2;
  }

  str += s.slice(start, end);

  while (i < args.length) {
    str += ' ' + formatValue(args[i++]);
  }

  return str;
}

function formatValue(v) {
  if (v === undefined) {
    return 'undefined';
  } else if (v === null) {
    return 'null';
  } else if (Array.isArray(v)) {
    return '[' + v.toString() + ']';
  } else if (v instanceof Error) {
    return v.toString();
  } else if (typeof v === 'object') {
    return JSON.stringify(v, null, 2);
  } else {
    return v.toString();
  }
}


function stringToNumber(value, default_value) {
  var num = Number(value);
  return isNaN(num) ? default_value : num;
}


function errnoException(err, syscall, original) {
  var errname = 'error'; // uv.errname(err);
  var message = syscall + ' ' + errname;

  if (original)
    message += ' ' + original;

  var e = new Error(message);
  e.code = errname;
  e.errno = errname;
  e.syscall = syscall;

  return e;
}


function exceptionWithHostPort(err, syscall, address, port, additional) {
  var details;
  if (port && port > 0) {
    details = address + ':' + port;
  } else {
    details = address;
  }

  if (additional) {
    details += ' - Local (' + additional + ')';
  }

  var ex = exports.errnoException(err, syscall, details);
  ex.address = address;
  if (port) {
    ex.port = port;
  }

  return ex;
}


exports.isNull = isNull;
exports.isUndefined = isUndefined;
exports.isNullOrUndefined = isNullOrUndefined;
exports.isNumber = isNumber;
exports.isBoolean = isBoolean;
exports.isString = isString;
exports.isObject = isObject;
exports.isFinite = isFinite;
exports.isFunction = isFunction;
exports.isBuffer = Buffer.isBuffer;
exports.isArray = Array.isArray;
exports.exceptionWithHostPort = exceptionWithHostPort;
exports.errnoException = errnoException;
exports.stringToNumber = stringToNumber;
exports.inherits = inherits;
exports.mixin = mixin;
exports.format = format;
exports.formatWithTag = formatWithTag;
