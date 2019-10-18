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


#include "iotjs_def.h"
#include "iotjs_module_i2c.h"
#include "iotjs_uv_request.h"
#include "iotjs_module_buffer.h"


IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(i2c);

IOTJS_DEFINE_PERIPH_CREATE_FUNCTION(i2c);

static void iotjs_i2c_destroy(iotjs_i2c_t* i2c) {
  iotjs_i2c_destroy_platform_data(i2c->platform_data);
  IOTJS_RELEASE(i2c);
}

static void i2c_worker(uv_work_t* work_req) {
  iotjs_periph_data_t* worker_data =
      (iotjs_periph_data_t*)IOTJS_UV_REQUEST_EXTRA_DATA(work_req);
  iotjs_i2c_t* i2c = (iotjs_i2c_t*)worker_data->data;

  switch (worker_data->op) {
    case kI2cOpOpen:
      worker_data->result = iotjs_i2c_open(i2c);
      break;
    case kI2cOpWrite:
      worker_data->result = iotjs_i2c_write(i2c);
      break;
    case kI2cOpReadByte:
      worker_data->result = iotjs_i2c_read_byte(i2c);
      break;
    case kI2cOpRead:
      worker_data->result = iotjs_i2c_read(i2c);
      break;
    case kI2cOpClose:
      worker_data->result = iotjs_i2c_close(i2c);
      break;
    default:
      IOTJS_ASSERT(!"Invalid Operation");
  }
}

JS_FUNCTION(I2cCons) {
  DJS_CHECK_THIS();
  DJS_CHECK_ARGS(1, object);
  DJS_CHECK_ARG_IF_EXIST(1, function);

  // Create I2C object
  const jerry_value_t ji2c = JS_GET_THIS();
  iotjs_i2c_t* i2c = i2c_create(ji2c);

  jerry_value_t jconfig;
  JS_GET_REQUIRED_ARG_VALUE(0, jconfig, IOTJS_MAGIC_STRING_CONFIG, object);

  jerry_value_t res = iotjs_i2c_set_platform_config(i2c, jconfig);
  if (jerry_value_is_error(res)) {
    return res;
  }

  JS_GET_REQUIRED_CONF_VALUE(jconfig, i2c->address, IOTJS_MAGIC_STRING_ADDRESS,
                             number);

  jerry_value_t jcallback = JS_GET_ARG_IF_EXIST(1, function);

  // If the callback doesn't exist, it is completed synchronously.
  // Otherwise, it will be executed asynchronously.
  if (!jerry_value_is_null(jcallback)) {
    iotjs_periph_call_async(i2c, jcallback, kI2cOpOpen, i2c_worker);
  } else if (!iotjs_i2c_open(i2c)) {
    return JS_CREATE_ERROR(COMMON, iotjs_periph_error_str(kI2cOpOpen));
  }

  return jerry_create_undefined();
}

JS_FUNCTION(Close) {
  JS_DECLARE_THIS_PTR(i2c, i2c);
  DJS_CHECK_ARG_IF_EXIST(1, function);

  iotjs_periph_call_async(i2c, JS_GET_ARG_IF_EXIST(0, function), kI2cOpClose,
                          i2c_worker);

  return jerry_create_undefined();
}

JS_FUNCTION(CloseSync) {
  JS_DECLARE_THIS_PTR(i2c, i2c);

  if (!iotjs_i2c_close(i2c)) {
    return JS_CREATE_ERROR(COMMON, iotjs_periph_error_str(kI2cOpClose));
  }

  return jerry_create_undefined();
}

static jerry_value_t i2c_write(iotjs_i2c_t* i2c, const jerry_value_t jargv[],
                               const jerry_length_t jargc, bool async) {
  const jerry_value_t jbuffer = JS_GET_ARG(0, object);
  iotjs_bufferwrap_t* buffer_wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);

  // Set buffer length and data from jarray
  i2c->buf_len = iotjs_bufferwrap_length(buffer_wrap);
  i2c->buf_data = buffer_wrap->buffer;

  if (async) {
    DJS_CHECK_ARG_IF_EXIST(1, function);
    iotjs_periph_call_async(i2c, JS_GET_ARG_IF_EXIST(1, function), kI2cOpWrite,
                            i2c_worker);
  } else {
    if (!iotjs_i2c_write(i2c)) {
      return JS_CREATE_ERROR(COMMON, iotjs_periph_error_str(kI2cOpWrite));
    }
  }

  return jerry_create_undefined();
}

typedef enum { IOTJS_I2C_WRITE, IOTJS_I2C_WRITESYNC } iotjs_i2c_op_t;

jerry_value_t i2c_do_write_or_writesync(const jerry_value_t jfunc,
                                        const jerry_value_t jthis,
                                        const jerry_value_t jargv[],
                                        const jerry_length_t jargc,
                                        const iotjs_i2c_op_t i2c_op) {
  JS_DECLARE_THIS_PTR(i2c, i2c);
  DJS_CHECK_ARGS(1, object);
  return i2c_write(i2c, jargv, jargc, i2c_op == IOTJS_I2C_WRITE);
}

JS_FUNCTION(Write) {
  return i2c_do_write_or_writesync(jfunc, jthis, jargv, jargc, IOTJS_I2C_WRITE);
}

JS_FUNCTION(WriteSync) {
  return i2c_do_write_or_writesync(jfunc, jthis, jargv, jargc,
                                   IOTJS_I2C_WRITESYNC);
}

JS_FUNCTION(ReadByte){
    JS_DECLARE_THIS_PTR(i2c, i2c);
    DJS_CHECK_ARGS(2, number, function);

    i2c->readCommand = jerry_get_number_value(jargv[0]);
    i2c->buf_len = 1;//(uint8_t )jerry_get_number_value(jargv[1]);
    jerry_value_t jbuffer = iotjs_bufferwrap_create_buffer(i2c->buf_len);
    iotjs_bufferwrap_t* wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);
    i2c->buf_data = wrap->buffer;
    i2c->jbuffer = jbuffer;

    iotjs_periph_call_async(i2c, JS_GET_ARG(1, function), kI2cOpReadByte,
                            i2c_worker);

    return jerry_create_undefined();
}


JS_FUNCTION(Read) {
  //printf("JERRYSCRIPT i2c read\n");
  JS_DECLARE_THIS_PTR(i2c, i2c);
  DJS_CHECK_ARGS(3, number, number, function);

  //printf("JERRYSCRIPT i2c read\n");
  i2c->readCommand = jerry_get_number_value(jargv[0]);
  i2c->buf_len = (uint8_t )jerry_get_number_value(jargv[1]);
  jerry_value_t jbuffer = iotjs_bufferwrap_create_buffer(i2c->buf_len);
  iotjs_bufferwrap_t* wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);
  i2c->buf_data = wrap->buffer;
  i2c->jbuffer = jbuffer;

  iotjs_periph_call_async(i2c, JS_GET_ARG(2, function), kI2cOpRead,
                          i2c_worker);

  return jerry_create_undefined();
}

JS_FUNCTION(ReadSync) {
  JS_DECLARE_THIS_PTR(i2c, i2c);
  DJS_CHECK_ARGS(1, number);

  i2c->buf_len = (uint8_t )jerry_get_number_value(jargv[0]);
  jerry_value_t jbuffer = iotjs_bufferwrap_create_buffer(i2c->buf_len);
  iotjs_bufferwrap_t* wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);
  i2c->buf_data = wrap->buffer;

  jerry_value_t result;
  if (iotjs_i2c_read(i2c)) {
    result = jbuffer;
  } else {
    result = JS_CREATE_ERROR(COMMON, iotjs_periph_error_str(kI2cOpRead));
    jerry_release_value(jbuffer);
  }

  return result;
}

jerry_value_t InitI2c() {
  jerry_value_t ji2c_cons = jerry_create_external_function(I2cCons);

  jerry_value_t prototype = jerry_create_object();

  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_CLOSE, Close);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_CLOSESYNC, CloseSync);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_WRITE, Write);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_WRITESYNC, WriteSync);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_READ, Read);
    iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_READBYTE, ReadByte);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_READSYNC, ReadSync);

  iotjs_jval_set_property_jval(ji2c_cons, IOTJS_MAGIC_STRING_PROTOTYPE,
                               prototype);

  jerry_release_value(prototype);

  return ji2c_cons;
}
