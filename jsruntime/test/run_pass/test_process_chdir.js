/* Copyright 2017-present Samsung Electronics Co., Ltd. and other contributors
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

var currentPath = process.cwd();

try {
  process.chdir('/');
} catch (err) {
  console.log('invalid path');
}

var newPath = process.cwd();
if (process.platform === "windows") {
  /* check if the path is in format: <Drive Letter>:\ */
  assert.equal(newPath.substr(1), ':\\');
} else {
  assert.equal(newPath, '/');
}

process.chdir(currentPath);
