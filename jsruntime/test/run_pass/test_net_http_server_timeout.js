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



var assert = require('assert');
var http = require('http');

var timeouted = false;

var server = http.createServer(function(req, res) {
  // do nothing
});

server.listen(3003);

server.setTimeout(100, function(socket) {
  timeouted = true;
  socket.destroy();
  server.close();
});


// client will connect to server, and do nothing.
var options = {
  method : 'GET',
  port : 3003
};


var getReq = http.request(options);
getReq.on('error', function() {});

process.on('exit', function(code) {
  assert.equal(timeouted, true);
  assert.equal(code, 0);
});

getReq.end();
