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


var util = require('util');


function Console() {
}

Console.prototype.useFileLog = function(params){
  native.open(params);
};

Console.prototype.log =
Console.prototype.info = function() {
  native.stdout(util.format.apply(this, arguments) + '\n');
};


Console.prototype.warn =
Console.prototype.error = function() {
  native.stderr(util.format.apply(this, arguments) + '\n');
};

class Log {
  constructor(tag){
    if(!util.isString(tag)){
      throw "tag not string";
    }
    this.tag = tag;
  }
  log(){
    native.stdout(util.formatWithTag.apply(this, arguments) + '\n');
  }
}

module.exports = new Console();
module.exports.Console = Console;
module.exports.Log = Log;
