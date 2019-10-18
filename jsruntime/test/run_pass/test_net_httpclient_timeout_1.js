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
var IncomingMessage = require('http_incoming').IncomingMessage;
var OutgoingMessage = require('http_outgoing').OutgoingMessage;
var net = require('net');

var timeouted = false;

var options = {
  method: 'GET',
  port: 3002
};

var incTimeout = 0;
var outTimeout = 0;

var socket = new net.Socket();
var inc = new IncomingMessage(socket);
inc.setTimeout(100, function() {
  incTimeout++;
});
var out = new OutgoingMessage();
out.setTimeout(100, function() {
  outTimeout++;
});
out.emit('timeout');

var server = http.createServer(function(req, res) {
  // do nothing
});

server.listen(options.port, function() {
  var req = http.request(options, function(res) {
  });
  req.on('close', function() {
    server.close();
  });
  var destroyer = function() {
    timeouted = true;
    req.socket.destroy();
  }
  req.setTimeout(100, destroyer);
  req.on('error', function(){});
  req.end();
});


process.on('exit', function(code) {
  assert.equal(code,0);
  assert.equal(timeouted, true);
  assert.equal(incTimeout, 1);
  assert.equal(outTimeout, 1);
});
