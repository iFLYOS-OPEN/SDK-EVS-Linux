/* Copyright 2018-present Samsung Electronics Co., Ltd. and other contributors
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

var shaTypes = {
  'sha1': 4,
  'sha256': 6,
};

var hashes = ['sha1', 'sha256'];

function Verify(signtype) {
  if (!(this instanceof Verify)) {
    return new Verify(signtype);
  }

  signtype = signtype.toLowerCase();

  if (hashes.indexOf(signtype) < 0) {
    throw new Error('Unknown signing algorithm.' +
                    'Please use crypto.getSignatures()');
  }

  Object.defineProperty(this, 'hashtype', {
    // defaults to writable: false, configurable: false
    value: signtype,
    enumerable: true,
  });
}

Verify.prototype.update = function(data) {
  if (this.data) {
    if (Buffer.isBuffer(data)) {
      this.data = Buffer.concat([this.data, data]);
      return;
    }

    this.data = Buffer.concat([this.data, new Buffer(data)]);
    return;
  }

  if (Buffer.isBuffer(data)) {
    this.data = data;
    return;
  }

  this.data = new Buffer(data);
};

Verify.prototype.verify = function(publicKey, signature) {
  if (this.data) {
    var type = shaTypes[this.hashtype];
    var hash = native.shaEncode(this.data, type);
    return native.rsaVerify(type, hash, publicKey, signature);
  }

  throw new Error('verify shouldn\'t be called on an empty Verify');
};

function Hash(hashtype) {
  if (!(this instanceof Hash)) {
    return new Hash(hashtype);
  }

  if (hashes.indexOf(hashtype) < 0) {
    throw new Error('Unknown hashing algorithm. Please use crypto.getHashes()');
  }
  Object.defineProperty(this, 'hashtype', {
    value: hashtype,
    writable: false,
    enumerable: true,
  });
}

Hash.prototype.update = function(data) {
  if (this.data) {
    if (Buffer.isBuffer(data)) {
      this.data = Buffer.concat(this.data, data);
      return;
    }

    this.data = Buffer.concat([this.data, new Buffer(data)]);
    return;
  }
  this.data = new Buffer(data);
};

Hash.prototype.digest = function(encoding) {
  if (this._finished) {
    throw new Error('Digest can not be called twice on the same Hash object');
  }

  var result;
  var type = shaTypes[this.hashtype];
  result = native.shaEncode(this.data, type);

  if (encoding == 'base64') {
    result = native.base64Encode(result);
  } else if (encoding == 'hex') {
    result = result.toString('hex');
  }

  Object.defineProperty(this, '_finished', {
    value: true,
    writable: false,
    enumerable: false,
  });

  return result;
};

function getHashes() {
  return hashes;
}

function createHash(hashtype) {
  return new Hash(hashtype);
}

function createVerify(signtype) {
  return new Verify(signtype);
}

exports.createHash = createHash;
exports.getHashes = getHashes;
exports.createVerify = createVerify;
