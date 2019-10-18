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

extern "C" {
#include "iotjs_def.h"
}

#include "iotjs_debuglog.h"
#include "iotjs_module_iFLYOS_wpacontroller.h"

struct iotjs_wpaclient_uvwork_t {
    std::string ssid;
    std::string password;
    jerry_value_t jcallback;
    bool result;
};

void iotjs_wpaclient_config(uv_work_t* req) {
    iotjs_wpaclient_uvwork_t *work = static_cast<iotjs_wpaclient_uvwork_t *>(req->data);
    work->result = iflyos::WpaController::instance().configWifi(work->ssid, work->password);
}

void iotjs_after_wpaclient_config(uv_work_t* req, int status) {
    iotjs_wpaclient_uvwork_t *work = static_cast<iotjs_wpaclient_uvwork_t *>(req->data);

    jerry_value_t jarg;
    if (!work->result) {
        jarg = jerry_get_value_from_error(jerry_create_error(JERRY_ERROR_COMMON, (const jerry_char_t *)"fail"), true);
    } else {
        jarg = jerry_create_null();
    }

    iotjs_invoke_callback(work->jcallback, jerry_create_undefined(), &jarg, 1);
    jerry_release_value(jarg);
    jerry_release_value(work->jcallback);
    delete work;
    delete req;
}

JS_FUNCTION(HasWifi) {
    return jerry_create_boolean(iflyos::WpaController::instance().hasWifi());
}

JS_FUNCTION(ConfigWifi) {
    JS_CHECK_ARGS(3, string, string, function)
    iotjs_string_t ssid = JS_GET_ARG(0, string);
    iotjs_string_t pswd = JS_GET_ARG(1, string);
    jerry_value_t jcallback = JS_GET_ARG(2, function);
    jerry_acquire_value(jcallback);

    auto req = new uv_work_t;
    auto work = new iotjs_wpaclient_uvwork_t;
    work->ssid = ssid.data;
    work->password = pswd.data ? pswd.data : "";
    work->jcallback = jcallback;
    req->data = static_cast<void *>(work);

    iotjs_string_destroy(&ssid);
    iotjs_string_destroy(&pswd);
    auto uv = iotjs_environment_loop(iotjs_environment_get());
    uv_queue_work(uv, req, iotjs_wpaclient_config, iotjs_after_wpaclient_config);
    return jerry_create_undefined();
}

constexpr int c_strcmp( char const* lhs, char const* rhs )
{
    return (('\0' == lhs[0]) && ('\0' == rhs[0])) ? 0
                                                  :  (lhs[0] != rhs[0]) ? (lhs[0] - rhs[0])
                                                                        : c_strcmp( lhs+1, rhs+1 );
}

extern "C" {
jerry_value_t InitWpaClient(void) {
    jerry_value_t wpaCli = jerry_create_object();

    iotjs_jval_set_method(wpaCli, "hasWifi", HasWifi);
    iotjs_jval_set_method(wpaCli, "configWifi", ConfigWifi);

    return wpaCli;
}
}
