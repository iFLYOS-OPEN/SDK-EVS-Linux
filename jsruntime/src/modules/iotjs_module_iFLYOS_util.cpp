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
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <openssl/sha.h>

#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

struct BgSystemWorkCxt{
    std::string cmd;
    int retCode;
    jerry_value_t cb;
};

static void systemWork(uv_work_t* work){
    auto workCxt = (BgSystemWorkCxt*)work->data;
    std::system(workCxt->cmd.c_str());
}

static void systemAfterWork(uv_work_t* work, int status){
    auto workCxt = (BgSystemWorkCxt*)work->data;
    iotjs_invoke_callback(workCxt->cb, jerry_create_undefined(), nullptr, 0);
    jerry_release_value(workCxt->cb);
    delete workCxt;
    delete work;
}

JS_FUNCTION(System) {
    JS_CHECK_ARGS(1, string)

    iotjs_string_t cmd = JS_GET_ARG(0, string);
    std::string cmdStr(cmd.data, cmd.size);
    iotjs_string_destroy(&cmd);

    if(jargc > 1 && jerry_value_is_function(jargv[1])){
        auto work = new uv_work_t;
        auto workCxt = new BgSystemWorkCxt;
        workCxt->cb = jerry_acquire_value(jargv[1]);
        workCxt->cmd = cmdStr;
        workCxt->retCode = 0;
        work->data = workCxt;

        uv_queue_work(uv_default_loop(), work, systemWork, systemAfterWork);
        return jerry_create_undefined();
    }

    auto res = std::system(cmdStr.c_str());
    return jerry_create_number(res);
}

JS_FUNCTION(GetWifiMac) {
    JS_CHECK_ARGS(1, string)
    iotjs_string_t name = JS_GET_ARG(0, string);

    char macAddr[13] = {0};
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifreq));
        strcpy(ifr.ifr_name, name.data);
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0) {
            for (size_t i = 0; i < 6; i++) {
                sprintf(macAddr + i * 2, "%02X", (unsigned char)ifr.ifr_hwaddr.sa_data[i]);
            }
        }
        close(fd);
    }
    iotjs_string_destroy(&name);
    return jerry_create_string((jerry_char_t *)macAddr);
}

JS_FUNCTION(Sha) {
    std::vector<std::string> params;
    jerry_value_t jvalue;
    for (int i = 0;; i++) {
        jvalue = JS_GET_ARG_IF_EXIST(i, string);
        if (jerry_value_is_null(jvalue)) {
            break;
        } else {
            auto str = iotjs_jval_as_string(jvalue);
            params.emplace_back(str.data);
            iotjs_string_destroy(&str);
        }
    }

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    unsigned char hash[SHA_DIGEST_LENGTH * 3];
    for (auto i = 0; i < params.size(); ++i) {
        SHA1_Update(&ctx, params[i].c_str(), params[i].length());
        if (i < params.size() - 1) {
            SHA1_Update(&ctx, ":", 1);
        }
    }
    SHA1_Final(hash, &ctx);
    for (auto i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(reinterpret_cast<char *>(&hash[SHA_DIGEST_LENGTH + i * 2]), "%02x", hash[i]);
    }
    return jerry_create_string(reinterpret_cast<const jerry_char_t *>(&hash[SHA_DIGEST_LENGTH]));
}

JS_FUNCTION(Rand) {
    srand(time(nullptr));
    return jerry_create_number(rand());
}

extern "C" {
jerry_value_t InitiFLYOSUtil(void) {
    jerry_value_t iflyosUtil = jerry_create_object();

    iotjs_jval_set_method(iflyosUtil, "system", System);
    iotjs_jval_set_method(iflyosUtil, "getWifiMac", GetWifiMac);
    iotjs_jval_set_method(iflyosUtil, "sha", Sha);
    iotjs_jval_set_method(iflyosUtil, "rand", Rand);
    return iflyosUtil;
}
}
