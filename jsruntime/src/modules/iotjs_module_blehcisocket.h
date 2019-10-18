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

/* Copyright (C) 2015 Sandeep Mistry sandeep.mistry@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef IOTJS_MODULE_BLE_HCI_SOCKET_H
#define IOTJS_MODULE_BLE_HCI_SOCKET_H

#include "iotjs_def.h"

typedef struct {
  jerry_value_t jobject;

  int _mode;
  int _socket;
  int _devId;
  uv_poll_t _pollHandle;
  int _l2sockets[1024];
  int _l2socketCount;
  uint8_t _address[6];
  uint8_t _addressType;
} iotjs_blehcisocket_t;


iotjs_blehcisocket_t* iotjs_blehcisocket_create(jerry_value_t jble);


void iotjs_blehcisocket_initialize(iotjs_blehcisocket_t* iotjs_blehcisocket);
void iotjs_blehcisocket_close(iotjs_blehcisocket_t* iotjs_blehcisocket);
void iotjs_blehcisocket_start(iotjs_blehcisocket_t* iotjs_blehcisocket);
int iotjs_blehcisocket_bindRaw(iotjs_blehcisocket_t* iotjs_blehcisocket,
                               int* devId);
int iotjs_blehcisocket_bindUser(iotjs_blehcisocket_t* iotjs_blehcisocket,
                                int* devId);
void iotjs_blehcisocket_bindControl(iotjs_blehcisocket_t* iotjs_blehcisocket);
bool iotjs_blehcisocket_isDevUp(iotjs_blehcisocket_t* iotjs_blehcisocket);
void iotjs_blehcisocket_setFilter(iotjs_blehcisocket_t* iotjs_blehcisocket,
                                  char* data, size_t length);
void iotjs_blehcisocket_poll(iotjs_blehcisocket_t* iotjs_blehcisocket);
void iotjs_blehcisocket_stop(iotjs_blehcisocket_t* iotjs_blehcisocket);
void iotjs_blehcisocket_write(iotjs_blehcisocket_t* iotjs_blehcisocket,
                              char* data, size_t length);
void iotjs_blehcisocket_emitErrnoError(
    iotjs_blehcisocket_t* iotjs_blehcisocket);
int iotjs_blehcisocket_devIdFor(iotjs_blehcisocket_t* iotjs_blehcisocket,
                                int* pDevId, bool isUp);
int iotjs_blehcisocket_kernelDisconnectWorkArounds(
    iotjs_blehcisocket_t* iotjs_blehcisocket, int length, char* data);

void iotjs_blehcisocket_poll_cb(uv_poll_t* handle, int status, int events);


#endif /* IOTJS_MODULE_BLE_HCI_SOCKET_H */
