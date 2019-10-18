/* The MIT License (MIT)
 *
 * Copyright (c) 2005-2014 RoadNarrows LLC.
 * http://roadnarrows.com
 * All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Copyright 2016-present Samsung Electronics Co., Ltd. and other contributors
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


/* Some functions are modified from the RoadNarrows-robotics i2c library.
 * (distributed under the MIT license.)
 */


#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "modules/iotjs_module_i2c.h"


#define I2C_SLAVE_FORCE 0x0706

struct iotjs_i2c_platform_data_s {
  iotjs_string_t device;
  int device_fd;
  uint8_t addr;
};

void iotjs_i2c_create_platform_data(iotjs_i2c_t* i2c) {
  i2c->platform_data = IOTJS_ALLOC(iotjs_i2c_platform_data_t);
  i2c->platform_data->device_fd = -1;
}

void iotjs_i2c_destroy_platform_data(iotjs_i2c_platform_data_t* pdata) {
  iotjs_string_destroy(&pdata->device);
  IOTJS_RELEASE(pdata);
}

jerry_value_t iotjs_i2c_set_platform_config(iotjs_i2c_t* i2c,
                                            const jerry_value_t jconfig) {
  iotjs_i2c_platform_data_t* platform_data = i2c->platform_data;

  JS_GET_REQUIRED_CONF_VALUE(jconfig, platform_data->device,
                             IOTJS_MAGIC_STRING_DEVICE, string);

  return jerry_create_undefined();
}


#define I2C_METHOD_HEADER(arg)                                   \
  iotjs_i2c_platform_data_t* platform_data = arg->platform_data; \
  IOTJS_ASSERT(platform_data);                                   \
  if (platform_data->device_fd < 0) {                            \
    DLOG("%s: I2C is not opened", __func__);                     \
    return false;                                                \
  }

bool iotjs_i2c_open(iotjs_i2c_t* i2c) {
  iotjs_i2c_platform_data_t* platform_data = i2c->platform_data;

  platform_data->device_fd =
      open(iotjs_string_data(&platform_data->device), O_RDWR);

  if (platform_data->device_fd == -1) {
    DLOG("%s : cannot open", __func__);
    return false;
  }

  if (ioctl(platform_data->device_fd, I2C_SLAVE_FORCE, i2c->address) < 0) {
    DLOG("%s : cannot set address", __func__);
    return false;
  }

  printf("%s : open success, addr %x \n", __func__, i2c->address);

  return true;
}

bool iotjs_i2c_close(iotjs_i2c_t* i2c) {
  I2C_METHOD_HEADER(i2c);

  if (close(platform_data->device_fd) < 0) {
    DLOG("%s : cannot close", __func__);
    return false;
  }

  platform_data->device_fd = -1;

  return true;
}

#include <sys/time.h>
static long currentTime(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    long time_in_mill =
            (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
    return time_in_mill;
}
bool iotjs_i2c_write(iotjs_i2c_t* i2c) {
  I2C_METHOD_HEADER(i2c);

  uint8_t len = i2c->buf_len;
  char* data = i2c->buf_data;

  //printf("%s : to write %d bytes\n", __func__, len);

  struct i2c_msg i2c_msg;
  i2c_msg.len = len;
  i2c_msg.addr = i2c->address;
  i2c_msg.flags = 0;
  i2c_msg.buf = (uint8_t*)data;

  struct i2c_rdwr_ioctl_data i2c_data;
  i2c_data.msgs = &i2c_msg;
  i2c_data.nmsgs = 1;

  //!!!do not remove the two times `currentTime();`
  long ctlStart = currentTime();
  int ret = ioctl(platform_data->device_fd, I2C_RDWR, (unsigned long)&i2c_data);
  long ctlEnd = currentTime();
  long duration = ctlEnd - ctlStart;
  if(duration > 4) {
      printf("long ioctl/write, [%ld, %ld] %ld\n", ctlStart, ctlEnd, duration);
  }
  if(ret < 0) {
      printf("%s : write %d bytes returns %d. errno:%d\n", __func__, len, ret, (int) errno);
  }

  return ret >= 0;
}

bool iotjs_i2c_read_byte(iotjs_i2c_t* i2c) {
    I2C_METHOD_HEADER(i2c);

    //printf("%s : to read 1 byte with command %x\n", __func__, i2c->buf_len, i2c->readCommand);
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data data;

    args.command = i2c->readCommand;
    args.read_write = I2C_SMBUS_READ;
    args.size = I2C_SMBUS_BYTE_DATA;
    args.data = &data;

    int ret = ioctl(platform_data->device_fd, I2C_SMBUS, &args);
    if (ret < 0) {
        printf("%s : read i2c one byte returns %d. errno:%d\n", __func__, ret, (int) errno);
        return false;
    }

    printf("%s : read 1 byte with command 0x%x, value: 0x%x, 0x%x, 0x%x \n", __func__, i2c->readCommand, data.byte, data.block[0], data.block[1]);

    i2c->buf_data[0] = (char)(0xff & (unsigned char)data.byte);

    return true;
}

bool iotjs_i2c_read(iotjs_i2c_t* i2c) {
  I2C_METHOD_HEADER(i2c);

  //printf("%s : to read %d bytes from %x\n", __func__, i2c->buf_len, i2c->readCommand);
  struct i2c_smbus_ioctl_data args;
  union i2c_smbus_data data;
  args.read_write = I2C_SMBUS_READ;
  args.size = I2C_SMBUS_BLOCK_DATA;//I2C_SMBUS_BYTE_DATA;
  args.data = &data;

  args.command = i2c->readCommand;
  int ret = ioctl(platform_data->device_fd, I2C_SMBUS, &args);
  if (ret < 0) {
    printf("%s : read i2c bytes returns %d. errno:%d\n", __func__, ret, (int) errno);
    return false;
  }

  for(int i = 0; i < i2c->buf_len; i++){
      i2c->buf_data[i] = (char)data.block[i];
  }

  return true;
}
