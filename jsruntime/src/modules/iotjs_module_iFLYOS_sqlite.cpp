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
#include "iotjs_module_buffer.h"

#include <memory>
#include <iostream>
#include <sqlite3.h>
#include <unordered_map>
#include <vector>

class Sqlite {
private:
    //int (*callback)(void*,int,char**,char**)
    static int exec_callback(void* context, int argc, char** values, char** cols){
        auto thiz = reinterpret_cast<Sqlite*>(context);
        std::unordered_map<std::string, std::string> result;
        for(auto i = 0; i < argc; i++) {
            result[std::string(cols[i])] = std::string(values[i] == nullptr ? "null" : values[i]);
        }
        thiz->m_queryResult.emplace_back(result);
        return 0;
    }

public:
    Sqlite(const std::string &db) : m_sqlite(nullptr) {
        sqlite3_open(db.c_str(), &m_sqlite);
    }

    ~Sqlite() {
        if (m_sqlite) {
            sqlite3_close(m_sqlite);
        }
    }

    bool isOpen() {
        return (m_sqlite);
    }

    int execute(const std::string &sql, std::string &data) {
        if(!isOpen()){
            std::cerr << "sqlite3 db not open" << std::endl;
            return -1;
        }
        char *errMsg = nullptr;
        m_queryResult.clear();
        auto res = sqlite3_exec(m_sqlite, sql.c_str(), exec_callback, reinterpret_cast<void*>(this), &errMsg);
        if (res != SQLITE_OK) {
            data = "null";
            if(errMsg){
                data = std::string(errMsg);
                sqlite3_free(errMsg);
            }

        }
        return res;
    }

    const std::vector<std::unordered_map<std::string, std::string>>& getResult() const{
        return m_queryResult;
    }

private:
    sqlite3 *m_sqlite;
    std::vector<std::unordered_map<std::string, std::string>> m_queryResult;
};

struct iotjs_sqlitewrap_t {
    std::shared_ptr<Sqlite> sqlite;
};

struct iotjs_sqlite_uvwork_t {
    std::shared_ptr<Sqlite> sqlite;
    jerry_value_t jcallback;
    int result;
    std::string data;
    std::string sql;
};

void iotjs_sqlite_execute(uv_work_t* req) {
    iotjs_sqlite_uvwork_t *work = static_cast<iotjs_sqlite_uvwork_t *>(req->data);
    work->result = work->sqlite->execute(work->sql, work->data);
}

void iotjs_sqlite_after_execute(uv_work_t* req, int status) {
    //if (status == UV_ECANCELED)
    iotjs_sqlite_uvwork_t *work = static_cast<iotjs_sqlite_uvwork_t *>(req->data);

    if (!jerry_value_is_null(work->jcallback)) {
        jerry_value_t jparam[2];
        if(work->result != SQLITE_OK) {
            jparam[0] = jerry_get_value_from_error(
                    jerry_create_error(JERRY_ERROR_COMMON,
                                       (const jerry_char_t *) (work->data.c_str())),
                    true);
            jparam[1] = jerry_create_undefined();

        }else{
            jparam[0] = jerry_create_undefined();
            auto result = work->sqlite->getResult();
            jparam[1] = jerry_create_array(result.size());
            auto i = 0;
            for(auto item : result){
                auto jItem = jerry_create_object();
                for(auto kv : item) {
                    iotjs_jval_set_property_string_raw(jItem, kv.first.c_str(), kv.second.c_str());
                }
                iotjs_jval_set_property_by_index(jparam[1], i++, jItem);
                jerry_release_value(jItem);
            }
        }

        iotjs_invoke_callback(work->jcallback, jerry_create_undefined(), jparam, 2);
        jerry_release_value(jparam[0]);
        jerry_release_value(jparam[1]);
        jerry_release_value(work->jcallback);
    }

    delete work;
    delete req;
}

IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(sqlitewrap);

static void iotjs_sqlitewrap_destroy(iotjs_sqlitewrap_t* sqlitewrap) {
    if (sqlitewrap->sqlite) {
        sqlitewrap->sqlite.reset();
    }

    IOTJS_RELEASE(sqlitewrap);
}

JS_FUNCTION(Open) {
    JS_CHECK_ARGS(2, object, string)

    const jerry_value_t jobject = JS_GET_ARG(0, object);
    iotjs_string_t file = JS_GET_ARG(1, string);

    auto sqlite = std::shared_ptr<Sqlite>(new Sqlite(file.data));
    iotjs_string_destroy(&file);
    if (!sqlite->isOpen()) {
        return jerry_create_boolean(false);
    }

    iotjs_sqlitewrap_t* sqlitewrap = (iotjs_sqlitewrap_t*)iotjs_buffer_allocate(sizeof(iotjs_sqlitewrap_t));
    sqlitewrap->sqlite = sqlite;
    jerry_set_object_native_pointer(jobject, sqlitewrap, &this_module_native_info);
    return jerry_create_boolean(true);
}

JS_FUNCTION(Execute) {
    JS_CHECK_ARGS(2, object, string)
    JS_DECLARE_OBJECT_PTR(0, sqlitewrap, sqlitewrap);
    auto sql = JS_GET_ARG(1, string);
    auto jcallback = JS_GET_ARG_IF_EXIST(2, function);
    if (!jerry_value_is_null(jcallback)) {
        jerry_acquire_value(jcallback);
    }

    auto req = new uv_work_t;
    iotjs_sqlite_uvwork_t *work = new iotjs_sqlite_uvwork_t;
    work->sqlite = sqlitewrap->sqlite;
    work->sql = sql.data;
    work->jcallback = jcallback;
    req->data = static_cast<void *>(work);
    iotjs_string_destroy(&sql);
    auto uv = iotjs_environment_loop(iotjs_environment_get());
    uv_queue_work(uv, req, iotjs_sqlite_execute, iotjs_sqlite_after_execute);
    return jerry_create_undefined();
}

extern "C" {
jerry_value_t InitSqlite(void) {
    jerry_value_t sqlite = jerry_create_object();

    iotjs_jval_set_method(sqlite, "open", Open);
    iotjs_jval_set_method(sqlite, "execute", Execute);
    return sqlite;
}
}
