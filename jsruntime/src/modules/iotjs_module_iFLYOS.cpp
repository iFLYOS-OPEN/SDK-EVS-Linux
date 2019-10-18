extern "C" {
#include "iotjs_def.h"
}

#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <atomic>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_set>
#include <iostream>
extern "C" {
#include "iotjs_module_buffer.h"
}

#include "iotjs_binding_cpp_helper.h"
#include "iotjs_module_iFLYOS_audio.h"

#define ROUTER_PORT_NAME "router"
#define MP_PORT_NAME "mediaplayer"

namespace iflyos{
namespace ipc {

#define MESSAGE_MAX (4096)
#define PARAM_MAX (1024)

typedef std::function<void(const std::string &name
    , const std::vector<std::shared_ptr<std::string>> &params)> Handler;

static int sock = -1;
static std::unordered_map<std::string, iflyos::ipc::Handler> handlers;
static std::unordered_map<std::string, std::unordered_set<std::string>> receivers;
static std::thread *receiver;
static struct sockaddr_un routerAddr;
static std::string routerAddrStr(ROUTER_PORT_NAME);
//NOTE one JS object never release. memory leak here
static std::shared_ptr<JHelper> jsRouterObj= nullptr;
static volatile int ivsPID{0};
static std::thread* mediaPlayerRunner{nullptr};
static std::thread* ivsProcessMonitor{nullptr};

bool sendBuffer(const std::string &dest, const char *buf, const int len) {
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memset(addr.sun_path, 0, sizeof(addr.sun_path));
    strcpy(&addr.sun_path[1], dest.c_str());

    auto wr = sendto(sock, buf, len, 0, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
    if (wr <= 0) {
        return false;
    }

    return true;
}

bool sendMessage(const std::string& dest, const std::string &name
    , const std::vector<std::string>& params) {
    auto nameCStr = name.c_str();

    if(name.length() >= MESSAGE_MAX - 1){
        return false;
    }

    char buf[MESSAGE_MAX];

    int idx = 0;
    memcpy(&buf[idx], nameCStr, name.length());
    idx += name.length();

    buf[idx] = ' ';
    idx++;

    for(auto p : params){
        idx += p.length() + sizeof(uint16_t);//'int16 length header'
        if(idx  >= sizeof(buf)){
            return false;
        }

        auto lenP = (uint16_t *)&buf[idx - p.length() - sizeof(uint16_t)];
        *lenP = p.length();

        memcpy(&buf[idx - p.length()], p.c_str(), p.length());
    }

    return sendBuffer(dest, buf, idx);
}

static void receiverThreadEntry() {
    char buf[MESSAGE_MAX];

    while (true) {
        auto rd = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (rd <= 0) {
            break;
        }
        //printf("IFLYOS_Interface receive message length:%ld\n", rd);

        buf[rd] = 0;
        int idx = 0;
        for(; idx < rd; idx++){
            if(buf[idx] == ' '){
                break;
            }
        }
        if(idx == rd){
            continue;
        }
        std::string name(buf, idx);
        idx++;
        std::vector<std::shared_ptr<std::string>> params(10);
        params.clear();
        while(idx < rd){
            int16_t * lenP = (int16_t *)&buf[idx];
            auto len = *lenP;
            if(len > PARAM_MAX){
                break;
            }
            idx += sizeof(uint16_t);

            if(idx + len > rd) {
                break;
            }

            params.emplace_back(std::make_shared<std::string>(&buf[idx], len));
            idx += len;

        }
        if(idx != rd){//has some error, not consume out buf
            continue;
        }
        auto handlerValue = handlers.end();
        handlerValue = handlers.find(name);
        if(handlerValue != handlers.end()) {
            if(name != "DO_REGISTER_EVT_RECEIVER"){
                //for debug break;
                std::cout << "router to process " << name << std::endl;
            }
            handlerValue->second(name, params);
        }

        auto receiverValue = receivers.find(name);
        if(receiverValue == receivers.end()){
            continue;
        }

        for(auto& dest : receiverValue->second) {
            sendBuffer(dest, buf, rd);
        }
    }
}

static void message_after_work_cb(uv_work_t *req, int){
    auto params = reinterpret_cast<std::vector<std::shared_ptr<std::string>>*>(req->data);
    auto ret = jsRouterObj->callMethod("_nativeCallback", *params);
    //auto isError = jerry_value_is_error(*ret);
    delete params;
    delete req;
}

static void message_work_cb(uv_work_t *req){

}

static void messageHandlerInSocketThread(const std::string &name
    , const std::vector<std::shared_ptr<std::string>> &params){
    auto req = new uv_work_t;
    auto allParams = new std::vector<std::shared_ptr<std::string>>(params);
    allParams->emplace(allParams->begin(), std::make_shared<std::string>(name));
    req->data = allParams;
    auto uv = iotjs_environment_loop(iotjs_environment_get());
    uv_queue_work(uv, req, message_work_cb, message_after_work_cb);
}

static void startIVS(const std::string& /*name*/, const std::vector<std::shared_ptr<std::string>>& params){
    if(params.size() < 1){
        sendMessage(ROUTER_PORT_NAME, "EVT_STOPPED", {"start param wrong"});
        return;
    }

//    if(ivsPID != 0) {
//        //TODO: rare condition. will not start IVSApp if IVSApp exit after this check
//        return;
//    }

    auto i = 0;
    auto mpExe = *params[i++];
//    auto ivsExe = *params[i++];
//    auto configPath = *params[i++];
//    auto voiceFIFO = *params[i++];
//    auto logLevel = *params[i++];

    //media player MUST clear resource when received EVT_STOPPED
    if(!mediaPlayerRunner){
        mediaPlayerRunner = new std::thread([mpExe]{
            std::system(mpExe.c_str());
        });
        //mediaplayer should stable and mediaPlayerRunner never quit
        //mediaPlayerRunner->detach();
        sleep(1);//wait media player run
    } else {
        //clear mediaplayer resources again, if not cleared
        sendMessage(MP_PORT_NAME, "EVT_STOPPED", {});
    }

#if false //no IVSApp for EVS
    auto pid = fork();
    if (pid == 0) {
        execl(ivsExe.c_str(), ivsExe.c_str(), "-C"
            , configPath.c_str(), "-V", voiceFIFO.c_str(), "-L", logLevel.c_str(), (char *) NULL);
        exit(-errno);
    }

    ivsPID = pid;
    ivsProcessMonitor = new std::thread([] {
        wait(nullptr);
        ivsPID = 0;
        sendMessage(ROUTER_PORT_NAME, "EVT_STOPPED", {});
    });
    ivsProcessMonitor->detach();
#endif
}

static void stopIVS(const std::string& /*name*/, const std::vector<std::shared_ptr<std::string>>& params){
    if(ivsPID != 0){
        kill(ivsPID, SIGTERM);
    }
}

static bool initIPCPort() {
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }

    int bufSize = MESSAGE_MAX;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    routerAddr.sun_family = AF_UNIX;
    memset(routerAddr.sun_path, 0, sizeof(routerAddr.sun_path));
    strcpy(&routerAddr.sun_path[1], ROUTER_PORT_NAME);

    handlers["DO_REGISTER_EVT_RECEIVER"] = [](const std::string& /*name*/, const std::vector<std::shared_ptr<std::string>> &params) {
        if(params.size() != 2){
            return;
        }

        auto evtName = params[0];
        if(*params[1] == ROUTER_PORT_NAME){
            handlers[*evtName] = messageHandlerInSocketThread;
        } else {
            auto kv = receivers.find(*evtName);
            if (kv == receivers.end()) {
                receivers[*evtName] = std::unordered_set<std::string>();
                kv = receivers.find(*evtName);
            }
            kv->second.emplace(*params[1]);
        }
        std::cout << *params[1] << " register event " << *evtName << std::endl;

        if(*evtName == "EVT_MEDIAPLAYER_CREATE") {
            messageHandlerInSocketThread("EVT_MEDIA_PLAYER_READY", {});
        }
    };

    handlers["DO_UNREGISTER_EVT_RECEIVER"] = [](const std::string& /*name*/, const std::vector<std::shared_ptr<std::string>> &params) {
        if(params.size() != 2){
            return;
        }

        auto evtName = params[0];
        if(*params[1] == ROUTER_PORT_NAME){
            handlers.erase(*evtName);
        }else {
            auto kv = receivers.find(*evtName);
            if (kv == receivers.end()) {
                return;
            }
            kv->second.erase(*params[1]);
        }
        std::cout << *params[1] << " unregister event " << *evtName << std::endl;
    };

    handlers["DO_START"] = &startIVS;
    handlers["DO_STOP"] = &stopIVS;

    /* Bind the UNIX domain address to the created socket */
    if (bind(sock, (struct sockaddr *) &routerAddr, sizeof(struct sockaddr_un))) {
        printf("bind error %d, %s\n", errno, strerror(errno));
        return false;
    }

    receiver = new std::thread(&receiverThreadEntry);
    return true;
}

//template<size_t ...Idx>
//static std::shared_ptr<std::vector<std::string>>> makeParamListN(
//    std::shared_ptr<JHelper> array
//    , std::integer_sequence<size_t, Idx...>){
//    return std::make_shared<std::vector<std::string>>({(JString::create(*array->getProperty(std::to_string(Idx))))->str()...});
//}

static void  makeParamList(std::shared_ptr<JHelper> array, std::vector<std::string>& ret) {
    auto len = jerry_get_array_length(*array);
    for(auto i = 0; i < len; i++){
        std::string v;
        array->getStringProperty(std::to_string(i), v);
        ret.emplace_back(v);
    }

//#define MP(n) \
//    case n: \
//        return makeParamListN(array, std::make_index_sequence<n>{})
//
//    auto len = jerry_get_array_length(*array);
//    switch(len){
//        MP(1);
//        MP(2);
//        MP(3);
//        MP(4);
//        MP(5);
//        MP(6);
//        MP(7);
//        MP(8);
//        MP(9);
//        default:
//            return {};
//    }
}

JS_FUNCTION(observe) {
    DJS_CHECK_ARG(0, string);
    auto name = JString::acquire(jargv[0]);
    //send self for reg
    sendMessage(routerAddrStr, "DO_REGISTER_EVT_RECEIVER", {*name, routerAddrStr});
    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(unobserve) {
    DJS_CHECK_ARGS(1, string);

    auto name = JString::acquire(jargv[0]);
    //send self for un-reg
    sendMessage(routerAddrStr, "DO_UNREGISTER_EVT_RECEIVER", {*name, routerAddrStr});
    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(jsSendMessage) {
    DJS_CHECK_ARGS(3, string, string, array);

    auto dest = JString::acquire(jargv[0]);
    auto name = JString::acquire(jargv[1]);
    auto jsParams = JHelper::acquire(jargv[2]);
//    auto len = jerry_get_array_length(*jsParams);
//    if(len > 10){
//        return JS_CREATE_ERROR(RANGE, "max 10 params");
//    }

    std::vector<std::string> params;
    makeParamList(jsParams, params);
    sendMessage(*dest, *name, params);
    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(bindJSRouter) {
    DJS_CHECK_ARG(0, object);

    if(!initIPCPort()) {
        return JS_CREATE_ERROR(URI, "init IPC port failed");
    }

    jsRouterObj = JHelper::acquire(jargv[0]);
    std::cout << "iFLYOS IPC JS Router obj:" << jargv[0] << std::endl;
    return JHelper::unmanagedUndefined();
}

/**
 * audio instance never GC-ed, but can stop(). so we not need delete the instances.
 *
 * write a native destructor callback not a problem actually. But, JerryScript not allow to get native pointer from
 * JS object with null jerry_object_native_info_t, making ALSASource_connect() having no way to find the sub-AudioSink
 * native object pointer
 */
//template <typename T>
//static void jsGCCallback(void* nativePointer) {
//    auto p = reinterpret_cast<T*>(nativePointer);
//    p->stop();
//    delete p;
//}
//
//static const jerry_object_native_info_t alsaSoundNI = {
//    &jsGCCallback<iflyos::audio::ALSASource>
//};
//static const jerry_object_native_info_t snowboyNI = {
//    &jsGCCallback<iflyos::audio::Snowboy>
//};
//static const jerry_object_native_info_t filewriterNI = {
//    &jsGCCallback<iflyos::audio::FileWriterSink>
//};

static void jsMemLeakGCCallback(void *nativePointer) {
    //never free audio instances once created, or let OS do
    assert(false);
}

static const jerry_object_native_info_t dummyNI ={
        &jsMemLeakGCCallback
};

JS_FUNCTION(ALSASource_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(4, string, number, number, boolean);

    auto jsThis = JHelper::acquire(jthis);
    auto devName = JString::acquire(jargv[0]);
    auto channelNum = jerry_get_number_value(jargv[1]);
    auto periodTime = jerry_get_number_value(jargv[2]);
    auto is32bit = jerry_get_boolean_value(jargv[3]);
    auto nativeThis = reinterpret_cast<void*>(iflyos::audio::ALSASource::create(devName->str(), channelNum, periodTime, is32bit));
    jerry_set_object_native_pointer(*jsThis, nativeThis, &dummyNI);

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(ALSASource_start){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object binded");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::ALSASource*>(notypeNativeThis);
    nativeThis->start();

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(ALSA_cset) {
    DJS_CHECK_ARGS(2, string, string);

    auto p1 = JString::acquire(jargv[0]);
    auto p2 = JString::acquire(jargv[1]);
    if(iflyos::audio::ALSASource::cset(*p1, *p2)){
        return JHelper::unmanagedUndefined();
    }
    return JS_CREATE_ERROR(EVAL, "cset failed");
}

JS_FUNCTION(ALSASource_connect){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, object);

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::ALSASource*>(notypeNativeThis);

    auto jsSink = JHelper::acquire(jargv[0]);
    void* notypeNativeSink;
    if(!jerry_get_object_native_pointer(*jsSink, &notypeNativeSink, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind to sink");
    }
    auto nativeSink = reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeSink);

    //std::cout << "ALSASource_connect" << reinterpret_cast<long>(nativeThis) << reinterpret_cast<long>(nativeSink) << std::endl;
    nativeThis->connectSink(nativeSink);

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(ALSASource_stop){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::ALSASource*>(notypeNativeThis);
    nativeThis->stop();

    return JHelper::unmanagedUndefined();
}

#ifdef R328PCMSOURCE

JS_FUNCTION(R328Source_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, number);

    auto jsThis = JHelper::acquire(jthis);
    auto channelNum = jerry_get_number_value(jargv[0]);
    auto nativeThis = reinterpret_cast<void*>(iflyos::audio::R328PCMSource::create(channelNum));
    jerry_set_object_native_pointer(*jsThis, nativeThis, &dummyNI);

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(R328Source_start){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object binded");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::R328PCMSource*>(notypeNativeThis);
    nativeThis->start();

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(R328Source_connect){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, object);

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::R328PCMSource*>(notypeNativeThis);

    auto jsSink = JHelper::acquire(jargv[0]);
    void* notypeNativeSink;
    if(!jerry_get_object_native_pointer(*jsSink, &notypeNativeSink, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind to sink");
    }
    auto nativeSink = reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeSink);

    //std::cout << "ALSASource_connect" << reinterpret_cast<long>(nativeThis) << reinterpret_cast<long>(nativeSink) << std::endl;
    nativeThis->connectSink(nativeSink);

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(R328Source_stop){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<iflyos::audio::R328PCMSource*>(notypeNativeThis);
    nativeThis->stop();

    return JHelper::unmanagedUndefined();
}

#endif //#ifdef R328PCMSOURCE

struct wakeup_c_context{
    std::shared_ptr<JHelper> jsObj;
    std::unordered_map<std::string, std::string> params;
};

static uv_async_t wakeupAsync;
static wakeup_c_context wakeupAsyncContext;

static void wakeupAsyncCB(uv_async_t */*req*/){
    auto jParamObj = JHelper::create(jerry_create_object());
    for(auto kv : wakeupAsyncContext.params) {
        iotjs_jval_set_property_string_raw(*jParamObj, kv.first.c_str(), kv.second.c_str());
    }
    wakeupAsyncContext.jsObj->callMethod("_nativeWakeup_callback", {jParamObj});
}

static void wakeupHandlerInWakeupThread(){
    printf("wakeupHandlerInWakeupThread() async send");
    uv_async_send(&wakeupAsync);
}

#ifdef SNOWBOY
JS_FUNCTION(Snowboy_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(2, string, string);

    auto modelPath = JString::acquire(jargv[0]);
    auto resPath = JString::acquire(jargv[1]);
    auto nativeThis = iflyos::audio::Snowboy::create(*modelPath, *resPath);
    auto jsThis = JHelper::acquire(jthis);
    uv_async_init(uv_default_loop(), &wakeupAsync, wakeupAsyncCB);
    wakeupAsync.data = &wakeupAsyncContext;
    wakeupAsyncContext.jsObj = jsThis;
    nativeThis->addObserver([jsThis/*NOTE: one time object memory leak*/]
    (const std::unordered_map<std::string, iflyos::audio::HotWordDetectorSink::HotWordDetectedParameterValue> params){
        wakeupHandlerInWakeupThread();
    });
    jerry_set_object_native_pointer(*jsThis, static_cast<iflyos::audio::AudioSink*>(nativeThis), &dummyNI);

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(Snowboy_start){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::Snowboy*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->start();

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(Snowboy_stop){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::Snowboy*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->stop();

    return JHelper::unmanagedUndefined();
}
#endif //SNOWBOY

#ifdef CAE
JS_FUNCTION(iFlytekCAE_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(2, string, number);

    static std::string kAngle("angle");
    static std::string kChannel("channel");
    static std::string kPower("power");
    static std::string kCMSCore("CMScore");
    static std::string kBeam("beam");

    auto resPath = JString::acquire(jargv[0]);
    auto channelNum = jerry_get_number_value(jargv[1]);
    auto nativeThis = iflyos::audio::IFlytekCAE::create(*resPath, channelNum);

    if(!nativeThis){
        return  JS_CREATE_ERROR(TYPE, "create cae native object failed");
    }
    /*NOTE: one time object memory leak*/
    auto jsThis = JHelper::acquire(jthis);
    uv_async_init(uv_default_loop(), &wakeupAsync, wakeupAsyncCB);
    wakeupAsync.data = &wakeupAsyncContext;
    wakeupAsyncContext.jsObj = jsThis;
    nativeThis->addObserver([jsThis]
    (const std::unordered_map<std::string, iflyos::audio::HotWordDetectorSink::HotWordDetectedParameterValue>& params){
        /**
         * params.emplace("angle", v1);int
         * params.emplace("channel", v2);int
         * params.emplace("power", v3);float
         * params.emplace("CMScore", v4);int
         * params.emplace("beam", v5);int
         */
        wakeupAsyncContext.params = {
            {"angle", std::to_string(params.at(kAngle).i)},
            {"channel", std::to_string(params.at(kChannel).i)},
            {"power", std::to_string(params.at(kPower).f)},
            {"CMScore", std::to_string(params.at(kCMSCore).i)},
            {"beam", std::to_string(params.at(kBeam).i)}
        };
        wakeupHandlerInWakeupThread();
    });
    jerry_set_object_native_pointer(*jsThis, static_cast<iflyos::audio::AudioSink*>(nativeThis), &dummyNI);

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(iFlytekCAE_start){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::IFlytekCAE*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->start();

    return JHelper::unmanagedUndefined();
}
JS_FUNCTION(iFlytekCAE_stop){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::IFlytekCAE*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->stop();

    return JHelper::unmanagedUndefined();
}

static std::thread reloadWorker;
JS_FUNCTION(iFlytekCAE_reload) {
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, string);

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::IFlytekCAE*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    iotjs_string_t value = JS_GET_ARG(0, string);
    std::string str(value.data, value.size);
    iotjs_string_destroy(&value);

    reloadWorker = std::thread([nativeThis, str]{
        printf("nativeThis->reload(%s);\n", str.c_str());
        nativeThis->reload(str);
    });
    reloadWorker.detach();

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(iFlytekCAE_auth) {
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, string);

    auto jsThis = JHelper::acquire(jthis);
    void *notypeNativeThis;
    if (!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)) {
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::IFlytekCAE *>(reinterpret_cast<iflyos::audio::AudioSink *>(notypeNativeThis));
    iotjs_string_t value = JS_GET_ARG(0, string);
    std::string str(value.data, value.size);
    iotjs_string_destroy(&value);

    nativeThis->auth(str);

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(iFlytekCAE_connect){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, object);

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::IFlytekCAE*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));

    auto jsSink = JHelper::acquire(jargv[0]);
    void* notypeNativeSink;
    if(!jerry_get_object_native_pointer(*jsSink, &notypeNativeSink, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind to sink");
    }
    auto nativeSink = reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeSink);

    nativeThis->connectSink(nativeSink);

    return JHelper::unmanagedUndefined();
}
#endif //CAE

JS_FUNCTION(FileWriterSink_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, string);

    auto jsThis = JHelper::acquire(jthis);
    auto filePath = JString::acquire(jargv[0]);
    auto nativeThis = iflyos::audio::FileWriterSink::create(*filePath);
    jerry_set_object_native_pointer(*jsThis, static_cast<iflyos::audio::AudioSink*>(nativeThis), &dummyNI);

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(FileWriterSink_start){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::FileWriterSink*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->start();

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(FileWriterSink_stop){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::FileWriterSink*>(reinterpret_cast<iflyos::audio::AudioSink*>(notypeNativeThis));
    nativeThis->stop();

    return JHelper::unmanagedUndefined();
}

template <typename T>
struct SimpleSPHolder{
    std::shared_ptr<T> m_pointer;
};

static void jsSDSReaderGCCallback(void *nativePointer) {
    auto nativeThis = reinterpret_cast<SimpleSPHolder<iflyos::audio::SDSReader>*>(nativePointer);
    delete nativeThis;
}

static const jerry_object_native_info_t SDSNI ={
        &jsSDSReaderGCCallback
};

JS_FUNCTION(SharedStreamSink_new){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, number);

    auto jsThis = JHelper::acquire(jthis);
    auto nativeThis = iflyos::audio::SharedStreamSink::create(jerry_get_number_value(jargv[0]));
    jerry_set_object_native_pointer(*jsThis, static_cast<iflyos::audio::AudioSink*>(nativeThis), &dummyNI);

    return JHelper::unmanagedUndefined();
}

JS_FUNCTION(SharedStreamReader_read){
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &SDSNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<SimpleSPHolder<iflyos::audio::SDSReader>*>(notypeNativeThis);
    auto jBuffer = iotjs_bufferwrap_create_buffer(640 * 2);
    auto buff_wrap = iotjs_bufferwrap_from_jbuffer(jBuffer);
    auto ret = nativeThis->m_pointer->read(buff_wrap->buffer, 320 * 2);
    if(ret > 0){
        buff_wrap->length = ret * 2;
        iotjs_jval_set_property_number(jBuffer, IOTJS_MAGIC_STRING_LENGTH, ret * 2);
        return jBuffer;
    }else{
        std::cout << "SDS read returns" << ret <<  std::endl;
        if(ret == iflyos::audio::SDSReader::Error::OVERRUN){
            //if overrun, skip to latest
            nativeThis->m_pointer->seek(0, iflyos::audio::SDSReader::Reference::BEFORE_WRITER);
        }
    }
    buff_wrap->length = 0;
    iotjs_jval_set_property_number(jBuffer, IOTJS_MAGIC_STRING_LENGTH, 0);
    return jBuffer;
}

JS_FUNCTION(SharedStream_close) {
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void* notypeNativeThis;
    if(!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &SDSNI)){
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = reinterpret_cast<SimpleSPHolder<iflyos::audio::SDSReader>*>(notypeNativeThis);
    nativeThis->m_pointer.reset();
    return jerry_create_undefined();
}

JS_FUNCTION(SharedStream_createReader) {
    DJS_CHECK_THIS();

    auto jsThis = JHelper::acquire(jthis);
    void *notypeNativeThis;
    if (!jerry_get_object_native_pointer(*jsThis, &notypeNativeThis, &dummyNI)) {
        return JS_CREATE_ERROR(TYPE, "no native object bind");
    }
    auto nativeThis = dynamic_cast<iflyos::audio::SharedStreamSink *>(reinterpret_cast<iflyos::audio::AudioSink *>(notypeNativeThis));
    auto jsReader = JHelper::acquire(jerry_create_object());
    iotjs_jval_set_method(*jsReader, "read", SharedStreamReader_read);
    iotjs_jval_set_method(*jsReader, "close", SharedStream_close);
    auto readerNative = new SimpleSPHolder<iflyos::audio::SDSReader>();
    readerNative->m_pointer = nativeThis->createReader();
    jerry_set_object_native_pointer(*jsReader, static_cast<void *>(readerNative), &SDSNI);
    return *jsReader;
}

//#include <linux/input.h>
//
//struct input_event* events;

extern "C" jerry_value_t InitiFLYOS(){
    signal(SIGPIPE, SIG_IGN);
    //printf("!!!sizeof input_event %d, offset of type %ld\n", sizeof(*events), (long)&events->type);
    auto iflyos = JHelper::acquire(jerry_create_object());
    iotjs_jval_set_method(*iflyos, "observe", observe);
    iotjs_jval_set_method(*iflyos, "unobserve", unobserve);
    iotjs_jval_set_method(*iflyos, "sendMessage", jsSendMessage);
    iotjs_jval_set_method(*iflyos, "bindJSRouter", bindJSRouter);
    iotjs_jval_set_method(*iflyos, "ALSASource_new", ALSASource_new);
    iotjs_jval_set_method(*iflyos, "ALSASource_start", ALSASource_start);
    iotjs_jval_set_method(*iflyos, "ALSASource_stop", ALSASource_stop);
    iotjs_jval_set_method(*iflyos, "ALSASource_connect", ALSASource_connect);
    iotjs_jval_set_method(*iflyos, "ALSA_cset", ALSA_cset);
    iotjs_jval_set_method(*iflyos, "SharedStreamSink_new", SharedStreamSink_new);
    iotjs_jval_set_method(*iflyos, "SharedStreamSink_createReader", SharedStream_createReader);
#ifdef SNOWBOY
    iotjs_jval_set_method(*iflyos, "Snowboy_new", Snowboy_new);
    iotjs_jval_set_method(*iflyos, "Snowboy_start", Snowboy_start);
    iotjs_jval_set_method(*iflyos, "Snowboy_stop", Snowboy_stop);
#endif

#ifdef CAE
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_new", iFlytekCAE_new);
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_start", iFlytekCAE_start);
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_stop", iFlytekCAE_stop);
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_connect", iFlytekCAE_connect);
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_reload", iFlytekCAE_reload);
    iotjs_jval_set_method(*iflyos, "iFlytekCAE_auth", iFlytekCAE_auth);
#endif

#ifdef R328PCMSOURCE
    iotjs_jval_set_method(*iflyos, "R328Source_new", R328Source_new);
    iotjs_jval_set_method(*iflyos, "R328Source_start", R328Source_start);
    iotjs_jval_set_method(*iflyos, "R328Source_stop", R328Source_stop);
    iotjs_jval_set_method(*iflyos, "R328Source_connect", R328Source_connect);
#endif//#ifdef R328PCMSOURCE

    iotjs_jval_set_method(*iflyos, "FileWriterSink_new", FileWriterSink_new);
    iotjs_jval_set_method(*iflyos, "FileWriterSink_start", FileWriterSink_start);
    iotjs_jval_set_method(*iflyos, "FileWriterSink_stop", FileWriterSink_stop);


#ifndef IFLYOS_PLATFORM
#error("IFLYOS_PLATFORM not defined")
#endif

#ifndef CAPTURE_CHANNEL_NUM
#define CAPTURE_CHANNEL_NUM 1
#endif

    iflyos->setProperty("platform", *JString::create(IFLYOS_PLATFORM));
    iflyos->setProperty("captureChannelNum", *JHelper::create(jerry_create_number(CAPTURE_CHANNEL_NUM)));

    return *iflyos;
}

}
}