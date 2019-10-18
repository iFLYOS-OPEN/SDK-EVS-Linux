#include "IFLYOSJHelper.h"
#include <thread>
#include <assert.h>

#ifndef NDEBUG
int JHelper::steadyObjId = 0;
std::list<int> JHelper::objs;
static std::thread::id jsRunningThread;
#endif

JHelper::JHelper(jerry_value_t v) : mValue{v} {
#ifndef NDEBUG
    if(!steadyObjId){
        //first time
        jsRunningThread = std::this_thread::get_id();
    } else {
        assert(jsRunningThread == std::this_thread::get_id());
    };
    myObjId = ++steadyObjId;
    objs.push_back(myObjId);
#endif
}

JHelper::~JHelper() {
#ifndef NDEBUG
    objs.remove(myObjId);
    assert(jsRunningThread == std::this_thread::get_id());
#endif
    jerry_release_value(mValue);
}

void JHelper::setProperty(jerry_value_t name, jerry_value_t value){
    auto v = jerry_set_property(*this, name, value);
    jerry_release_value(v);
}

JHelper::operator const jerry_value_t() const {
    return mValue;
}

std::shared_ptr<JHelper> JHelper::getProperty(const std::string &name) const{
    auto n = jerry_create_string((const jerry_char_t *)(name.c_str()));
    auto v = jerry_get_property(*this, n);
    jerry_release_value(n);
    return JHelper::create(v);
}

void JHelper::setProperty(const std::string &name, jerry_value_t value) {
    auto n = jerry_create_string((const jerry_char_t *)(name.c_str()));
    auto v = jerry_set_property(*this, n, value);
    jerry_release_value(v);
    jerry_release_value(n);
}

void JHelper::setProperty(const std::string &name, jerry_external_handler_t fun) {
    auto n = jerry_create_string((const jerry_char_t *)(name.c_str()));
    auto f = jerry_create_external_function(fun);
    auto v = jerry_set_property(*this, n, f);
    jerry_release_value(v);
    jerry_release_value(n);
    jerry_release_value(f);
}

jerry_value_t JHelper::unmanagedError(jerry_error_t t, const std::string &msg) {
    return jerry_create_error(t, (const jerry_char_t *)msg.c_str());
}


std::shared_ptr<JHelper> JHelper::create(jerry_value_t t) {
    return std::shared_ptr<JHelper>(new JHelper(t));
}

std::shared_ptr<JHelper> JHelper::invoke(const std::initializer_list<std::shared_ptr<JHelper>>& paramsList) {
    return invoke(std::vector<std::shared_ptr<JHelper>>(paramsList));
}

std::shared_ptr<JHelper> JHelper::invoke(const std::vector<std::shared_ptr<JHelper>>& paramsList) {
    if(!jerry_value_is_function(mValue)){
        throw "not a function";
    }
    jerry_value_t params[paramsList.size()];
    size_t  i = 0;
    for(auto &p : paramsList){
        params[i++] = *p;
    }
    return JHelper::create(jerry_call_function(mValue, *undefined(), params, paramsList.size()));
}


std::shared_ptr<JHelper> JHelper::invoke(const std::vector<std::string>& paramsList) {
    if(!jerry_value_is_function(mValue)){
        throw "not a function";
    }
    jerry_value_t params[paramsList.size() + 1];
    size_t  i = 0;
    for(auto &p : paramsList){
        params[i++] = *JString::create(p);
    }
    return JHelper::create(jerry_call_function(mValue, *undefined(), params, paramsList.size()));
}

bool JHelper::isError() {
    return jerry_value_is_error(mValue);
}

jerry_value_t JHelper::unmanagedUndefined() {
    return jerry_create_undefined();
}

std::shared_ptr<JHelper> JHelper::undefined() {
    return JHelper::create(jerry_create_undefined());
}

std::string JHelper::toString() {
    auto js = JString::create(jerry_value_to_string(mValue));
    return std::string(js->str());
}

std::shared_ptr<JHelper> JHelper::acquire(jerry_value_t t) {
    return create(jerry_acquire_value(t));
}

std::shared_ptr<JHelper> JHelper::promise() {
    return create(jerry_create_promise());
}

JString::JString(jerry_value_t v, const std::string& str) : JHelper(v), mValueStr{str}{
}

JString::operator const std::string &() const {
    return mValueStr;
}

JString::operator const char *() const {
    return mValueStr.c_str();
}

std::shared_ptr<JString> JString::create(jerry_value_t t) {
    if(!jerry_value_is_string(t)){
        throw "not string";
    }
    auto size = jerry_get_string_length(t);
    jerry_char_t buf[size + 1];
    jerry_string_to_char_buffer(t, buf, size);
    buf[size] = '\0';
    return std::shared_ptr<JString>(new JString(t, std::string((char*)buf)));
}

std::shared_ptr<JString> JString::create(const std::string &v) {
    auto t = jerry_create_string((const jerry_char_t*)v.c_str());
    return std::shared_ptr<JString>(new JString(t, v));
}

const std::string &JString::str() const {
    return mValueStr;
}

std::shared_ptr<JString> JString::acquire(jerry_value_t t) {
    return create(jerry_acquire_value(t));
}

