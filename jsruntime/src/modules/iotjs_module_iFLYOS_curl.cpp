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
#include "iotjs_debuglog.h"
#include "iotjs_module_buffer.h"
}

#include "iotjs_module_iFLYOS_threadpool.h"
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <iostream>
#include <curl/curl.h>

static auto currentMS(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    auto time_in_mill =
            (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
    return time_in_mill;
}

//负责实际工作的c++类封装
class EasyCurl {
public:
    //if finish, data is null, size is error code
    typedef std::function<bool(void* data, int size, bool finish)> Receiver;

    //创建easy curl实例
    EasyCurl(Receiver receiver) :
    m_curl(nullptr),
    m_dataReceiver(receiver),
    m_cancel{false} {
        if (!m_bGlobalInited) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            m_bGlobalInited = true;
        }
        m_curl = curl_easy_init();
    }

    //销毁easy curl实例
    ~EasyCurl() {
        //printf("~EasyCurl\n");
        if (m_curl) {
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
        }
    }

    //保存curl头,在执行操作的时候会通过curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers)设置进去
    void addHeader(const std::string& header){
        m_headers.emplace_back(header);
    }

    //将动态申请的c++对象生命周期绑定到easy curl对象,这样在对象销毁的时候就可以同时销毁动态申请的c++对象
    void managePointer(std::shared_ptr<void> p){
        m_pointers.emplace_back(p);
    }

    //设置libcurl参数
    CURLcode setOpt(CURLoption option, long val) {
        return curl_easy_setopt(m_curl, option, val);
    }

    //设置libcurl参数
    CURLcode setOpt(CURLoption option, const char *val) {
        if(option == CURLOPT_URL){
            //printf("url:%s\n", val);
        }
        return curl_easy_setopt(m_curl, option, val);
    }

    //url字符串编码
    void escape(std::string& inout){
        auto escaped = curl_easy_escape(m_curl, inout.c_str(), inout.length());
        inout = std::string(escaped);
        curl_free(escaped);
    }

    //放弃libcurl的返回
    void cancel(){
        m_cancel = true;
    }

    //是否放弃libcurl的返回
    bool isCanceled(){
        return m_cancel;
    }

    //执行
    CURLcode perform() {
        struct curl_slist *headers = NULL;
        for(auto& h : m_headers) {
            headers = curl_slist_append(headers, h.c_str());
        }
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, EasyCurl::writeCb);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1);
#ifdef R328PCMSOURCE
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif
        auto ret = curl_easy_perform(m_curl);
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
        if(ret != CURLE_OK){
            curl_global_cleanup();
            usleep(1000 * 100);
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
        curl_slist_free_all(headers);
        onEnd(ret);
        return ret;
    }

    void onEnd(CURLcode code){
        if(!m_dataReceiver) return;
        m_dataReceiver(nullptr, code, true);
    }

    static size_t writeCb(void *ptr, size_t size, size_t count, void *cxt) {
        auto thiz = (EasyCurl*)cxt;
        auto bytes = size * count;
        if(thiz->isCanceled()) return 0;
        if(bytes == 0) return 0;
        if(thiz->m_dataReceiver(ptr, bytes, false)){
            return bytes;
        }else{
            return 0;
        }
    }

private:
    CURL *m_curl;
    Receiver m_dataReceiver;
    std::vector<std::string> m_headers;
    std::vector<std::shared_ptr<void>> m_pointers;//和EasyCurl对象绑定销毁的其他c++对象
    volatile bool m_cancel;

    static bool m_bGlobalInited;
};

bool EasyCurl::m_bGlobalInited = false;

//////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct SPHolder{
    std::shared_ptr<T> p;
};

class JSStreamReceiver;

/**
 * 1. we can not close m_uvAsync in destructor,  because closing may still in progress after destructor returns and the m_uvAsync memory have been collected by cpp
 * 2. there is a bug, if uv_close() directly after uv_async_init() without uv_async_send(), libtuv makes a assert
 */
class JSBufferReceiver{
public:
    static void asyncClose(uv_handle_t* async){
        //printf("!!!!asyncClose");
        auto asyncData = (SPHolder<JSBufferReceiver>*)async->data;
        if(asyncData) {
            asyncData->p.reset();
            delete asyncData;
        }
    }
    static void asyncCB(uv_async_t* async){
        auto asyncData = (SPHolder<JSBufferReceiver>*)async->data;
        auto thiz = asyncData->p;
        //printf("!!!!asyncCB, thiz:%ld\n", (long)async->data);
        if(thiz->m_finish) {
            if (thiz->m_errCode != 0)
                thiz->_onErrorInJSThread(thiz->m_errCode);
            else
                thiz->_onFinishInJSThread();
            //move to destructor
            jerry_release_value(thiz->m_jsCallback);
            assert(std::this_thread::get_id() == thiz->m_mainId);
            //printf("!!!!uv_close() %p %ld\n", &thiz->m_uvAsync, currentMS());
            uv_close((uv_handle_t *) &thiz->m_uvAsync, asyncClose);
            return;
        }

        thiz->_onDataInJSThread();
    }

    virtual ~JSBufferReceiver(){
        //printf("~JSBufferReceiver\n");
        //destructor should be invoked in js thread
        //iotjs_curlwrap_destroy(){curlwrap->receiver.reset();}
    }

    JSBufferReceiver() :
    m_jsCallback{0}, m_data{new std::vector<char>()}, m_errCode{0}, m_finish{false}
    , m_cancel{false}, m_mainId{std::this_thread::get_id()}{
        //printf("!!!!uv_async_init()\n");
        m_uvAsync.data = nullptr;
        m_uvAsync.loop = nullptr;
    }

    //can not call uv_close after uv_async_init, or libuv assert, maybe bug?
    static void initAsync(std::shared_ptr<JSBufferReceiver> thiz){
        assert(std::this_thread::get_id() == thiz->m_mainId);
        //printf("!!!!uv_async_init() %ld\n", currentMS());
        uv_async_init(iotjs_environment_loop(iotjs_environment_get()), &thiz->m_uvAsync, asyncCB);
        auto asyncData = new SPHolder<JSBufferReceiver>();
        asyncData->p = thiz;
        thiz->m_uvAsync.data = asyncData;
    }

    void setJSCallback(jerry_value_t v){
        if(m_jsCallback != 0){
            IOTJS_ASSERT(false);
        }
        m_jsCallback = jerry_acquire_value(v);
    }

    static bool onCurlData(std::shared_ptr<JSBufferReceiver> thiz, void* data, int size, bool finish){
        if(finish){
            thiz->m_errCode = size;
            thiz->m_finish = true;
            //printf("!!!uv_async_send() %ld\n", currentMS());
            uv_async_send(&thiz->m_uvAsync);
            return true;
        }
        return thiz->_onDataInCURLThread((char *) data, size);
    }

    void cancel(){
        this->m_cancel = true;
    }

protected:
    virtual bool _onDataInCURLThread(char *data, int size){
        if(m_cancel){
            return false;
        }
        m_data->insert(m_data->end(), data, data + size);
        return true;
    }

    virtual void _onDataInJSThread(){

    }

    virtual void _onFinishInJSThread(){
        if(m_cancel){
            return;
        }
        jerry_value_t p[3];
        p[0] = jerry_create_undefined();
        p[1] = jerry_create_string_sz((const jerry_char_t*)m_data->data(), m_data->size());
        p[2] = jerry_create_boolean(true);

        iotjs_invoke_callback(m_jsCallback, jerry_create_undefined(), p, 3);

        jerry_release_value(p[0]);
        jerry_release_value(p[1]);
        jerry_release_value(p[2]);
    }

    virtual void _onErrorInJSThread(int errCode){
        if(m_cancel){
            return;
        }
        std::string errStr = curl_easy_strerror((CURLcode)errCode);
        if(errStr.empty()){
            errStr = errStr + "code:" + std::to_string(errCode);
        }
        auto jErr = jerry_get_value_from_error(
                jerry_create_error(JERRY_ERROR_COMMON, (const jerry_char_t *) errStr.c_str()),
                true);

        iotjs_invoke_callback(m_jsCallback, jerry_create_undefined(), &jErr, 1);
        jerry_release_value(jErr);
    }

protected:
    std::shared_ptr<std::vector<char>> m_data;
    uv_async_t m_uvAsync;
    jerry_value_t m_jsCallback;
    int m_errCode;
    std::atomic_bool m_finish;
    std::atomic_bool m_cancel;
    std::thread::id m_mainId;
};

class JSStreamReceiver : public JSBufferReceiver{
public:
    virtual void _onDataInJSThread(){
        if(m_cancel){
            return;
        }
        std::shared_ptr<std::vector<char>> data;
        {
            std::lock_guard<std::mutex> lock(m_dataLock);
            data = m_data;
            m_data = std::make_shared<std::vector<char>>();
        }
        if(data->size() == 0){
            return;
        }
        auto jBuffer = iotjs_bufferwrap_create_buffer(data->size());
        auto bufferWrap = iotjs_bufferwrap_from_jbuffer(jBuffer);
        iotjs_bufferwrap_copy(bufferWrap, data->data(), data->size());

        jerry_value_t p[2];
        p[0] = jerry_create_undefined();//no error
        p[1] = jBuffer;//data

        iotjs_invoke_callback(m_jsCallback, jerry_create_undefined(), p, 2);

        jerry_release_value(p[0]);
        jerry_release_value(p[1]);
    }

    virtual bool _onDataInCURLThread(char *data, int size) override{
        if(m_cancel){
            return false;
        }
        std::shared_ptr<std::vector<char>> dataSP = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_dataLock);
            m_data->insert(m_data->end(), data, data + size);
        }
        uv_async_send(&m_uvAsync);
        return true;
    }

    virtual void _onFinishInJSThread() override {
        if(m_cancel){
            return;
        }
        if(m_data->size() > 0){
            this->_onDataInJSThread();
        }
        JSBufferReceiver::_onFinishInJSThread();
    }

private:
    std::mutex m_dataLock;
};

//和js对象绑定的c结构体
struct iotjs_curlwrap_t {
    std::shared_ptr<EasyCurl> curl;
    std::shared_ptr<JSBufferReceiver> receiver;
};

//在worker线程执行的函数
void iotjs_curl_perform(uv_work_t* req) {
    auto *nativeObj = static_cast<SPHolder<EasyCurl> *>(req->data);
    nativeObj->p->perform();
}

//工作线程完成任务后,在js线程执行的函数
void iotjs_after_curl_perform(uv_work_t* req, int status) {
    auto *nativeObj = static_cast<SPHolder<EasyCurl>  *>(req->data);
    nativeObj->p.reset();
    delete nativeObj;
    delete req;
}

//定义一个用于JerryScript从js对象获取c对象的结构体 jerry_object_native_info_t
//这个结构体会引用 iotjs_curlwrap_destroy
IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(curlwrap);

//跟js对象绑定的c对象的销毁函数,在js对象被GC回收时会被执行
static void iotjs_curlwrap_destroy(iotjs_curlwrap_t* curlwrap) {
    //printf("iotjs_curlwrap_destroy\n");
    if (curlwrap->curl) {
        curlwrap->curl.reset();
    }

    if (curlwrap->receiver){
        curlwrap->receiver.reset();
    }

    IOTJS_RELEASE(curlwrap);
}
static void _dd(uv_async_t*){

}
//为js对象初始化c对象
JS_FUNCTION(Init) {
    //检查第一个参数是js对象类型
    JS_CHECK_ARGS(2, object, boolean)

    bool stream = JS_GET_ARG(1, boolean);
    //拿到js对象引用
    const jerry_value_t jobject = JS_GET_ARG(0, object);
    //创建c对象
    iotjs_curlwrap_t* curlwrap = (iotjs_curlwrap_t*)iotjs_buffer_allocate(sizeof(iotjs_curlwrap_t));
#if true
    std::shared_ptr<JSBufferReceiver> receiver = nullptr;
    if(stream) {
        receiver = std::make_shared<JSStreamReceiver>();
    }else {
        receiver = std::make_shared<JSBufferReceiver>();
    }
    curlwrap->curl = std::shared_ptr<EasyCurl>(new EasyCurl([receiver](void* data, int size, bool finish){
        return JSBufferReceiver::onCurlData(receiver, data, size, finish);
    }));
    curlwrap->receiver = receiver;
#else
    curlwrap->receiver = nullptr;
    curlwrap->curl = nullptr;
#endif

    //将c对象绑定到js对象
    jerry_set_object_native_pointer(jobject, curlwrap, &this_module_native_info);
    //printf("jerry_set_object_native_pointer\n");
    return jerry_create_undefined();
}

//添加头
JS_FUNCTION(AddHeader){
    //检查第一个参数是js对象类型,第二个参数是js字符串类型
    JS_CHECK_ARGS(2, object, string);

    //从js对象拿到c结构体
    JS_DECLARE_OBJECT_PTR(0, curlwrap, curlwrap);
    iotjs_string_t value = JS_GET_ARG(1, string);
    curlwrap->curl->addHeader(std::string(value.data, value.size));
    iotjs_string_destroy(&value);
    return jerry_create_undefined();
}

//设置curl参数
JS_FUNCTION(SetOpt) {
    JS_CHECK_ARGS(3, object, number, any)

    JS_DECLARE_OBJECT_PTR(0, curlwrap, curlwrap);
    int option = JS_GET_ARG(1, number);

    if (jerry_value_is_number(jargv[2])) {
        int value = JS_GET_ARG(2, number);
        auto setOptRet = curlwrap->curl->setOpt((CURLoption)option, value);
        if(setOptRet != CURLE_OK){
            std::cerr << "set opt failed>" << option << ":" << value << std::endl;
        }
        return jerry_create_boolean(setOptRet == CURLE_OK);
    } else if (jerry_value_is_string(jargv[2])) {
        iotjs_string_t value = JS_GET_ARG(2, string);
        auto managedStr = std::make_shared<std::string>(value.data, value.size);
        iotjs_string_destroy(&value);
        auto setOptRet = curlwrap->curl->setOpt((CURLoption)option, managedStr->c_str());
        if(setOptRet != CURLE_OK){
            std::cerr << "set opt failed>" << option << ":" << value.data << std::endl;
        }
        //如果设置成功返回true的js类型,否则false
        auto ret = jerry_create_boolean(setOptRet == CURLE_OK);
        if((CURLoption)option == CURLOPT_POSTFIELDS ){
            //因为libcurl在设置CURLOPT_POSTFIELDS的时候是不会拷贝一份数据的,这个字符串的生命周期需要持续到perform到以后,所以将这个字符串对象绑定到curl对象
            curlwrap->curl->managePointer(managedStr);
        }
        return ret;
    }

    return jerry_create_boolean(false);
}

//执行curl请求
JS_FUNCTION(Perform) {
    JS_CHECK_ARGS(2, object, function)

    JS_DECLARE_OBJECT_PTR(0, curlwrap, curlwrap);
    jerry_value_t jcallback = JS_GET_ARG(1, function);
    //需要在perform函数返回后继续持有js回调对象的引用,需要调用jerry_acquire_value告诉js GC不要释放
    curlwrap->receiver->setJSCallback(jcallback);

    auto req = new uv_work_t;
    auto work = new SPHolder<EasyCurl>();
    work->p = curlwrap->curl;
    req->data = work;

    JSBufferReceiver::initAsync(curlwrap->receiver);

    auto uv = iotjs_environment_loop(iotjs_environment_get());
    //通过libuv在worker线程中执行真正的curl请求动作iotjs_curl_perform,并制定执行完以后在js线程中执行的回调iotjs_after_curl_perform
    uv_queue_work(uv, req, iotjs_curl_perform, iotjs_after_curl_perform);
    return jerry_create_undefined();
}

JS_FUNCTION(Escape) {
    JS_CHECK_ARGS(1, string);

    iotjs_string_t value = JS_GET_ARG(0, string);
    std::string str(value.data, value.size);
    EasyCurl easyCurl(nullptr);
    easyCurl.escape(str);
    iotjs_string_destroy(&value);

    return jerry_create_string_sz_from_utf8((const jerry_char_t *)str.c_str(), str.length());
}

JS_FUNCTION(Cancel) {
    JS_CHECK_ARGS(1, object)

    JS_DECLARE_OBJECT_PTR(0, curlwrap, curlwrap);

    curlwrap->receiver->cancel();
    curlwrap->curl->cancel();
    return jerry_create_undefined();
}

JS_FUNCTION(GC){
    jerry_gc(JERRY_GC_SEVERITY_LOW);

    return jerry_create_undefined();
}


//返回一个js对象,到curl.js里面会成为native对象
extern "C" {
jerry_value_t InitCurl(void) {
    //c里面创建的js对象如果返回出去是不用释放引用的
    jerry_value_t curl = jerry_create_object();

    auto global = jerry_get_global_object();
    iotjs_jval_set_method(global, "_gc", GC);
    jerry_release_value(global);

    iotjs_jval_set_method(curl, "init", Init);
    iotjs_jval_set_method(curl, "addHeader", AddHeader);
    iotjs_jval_set_method(curl, "setOpt", SetOpt);
    iotjs_jval_set_method(curl, "perform", Perform);
    iotjs_jval_set_method(curl, "cancel", Cancel);
    iotjs_jval_set_method(curl, "escape", Escape);

    return curl;
}
}
