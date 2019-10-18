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


#ifndef IOTJS_MODULE_I2C_H
#define IOTJS_MODULE_I2C_H

#include "iotjs_def.h"
#include "iotjs_module_periph_common.h"

// Forward declaration of platform data. These are only used by platform code.
// Generic I2C module never dereferences platform data pointer.
typedef struct iotjs_i2c_platform_data_s iotjs_i2c_platform_data_t;
// This I2c class provides interfaces for I2C operation.
typedef struct {
  jerry_value_t jobject;
  iotjs_i2c_platform_data_t* platform_data;

  char* buf_data;
  uint8_t buf_len;
  int readCommand;
  uint8_t address;
  jerry_value_t jbuffer;
} iotjs_i2c_t;

jerry_value_t iotjs_i2c_set_platform_config(iotjs_i2c_t* i2c,
                                            const jerry_value_t jconfig);
bool iotjs_i2c_open(iotjs_i2c_t* i2c);
bool iotjs_i2c_write(iotjs_i2c_t* i2c);
bool iotjs_i2c_read(iotjs_i2c_t* i2c);
bool iotjs_i2c_read_byte(iotjs_i2c_t* i2c);
bool iotjs_i2c_close(iotjs_i2c_t* i2c);

// Platform-related functions; they are implemented
// by platform code (i.e.: linux, nuttx, tizen).
void iotjs_i2c_create_platform_data(iotjs_i2c_t* i2c);
void iotjs_i2c_destroy_platform_data(iotjs_i2c_platform_data_t* platform_data);

#endif /* IOTJS_MODULE_I2C_H */
