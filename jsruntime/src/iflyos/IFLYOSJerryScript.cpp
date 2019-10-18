#include <jerryscript.h>
#include <uv.h>
#include <sstream>
#include "IFLYOSJHelper.h"
#include "IFLYOSInterface.h"
#include "IFLYOSJerryScript.h"

static jerry_value_t initGlobalIPCPort(const jerry_value_t func_value,
                      const jerry_value_t this_value,
                      const jerry_value_t *args_p,
                      const jerry_length_t args_cnt){
    if (!jerry_value_is_string(*args_p)) {
        return JHelper::unmanagedError(JERRY_ERROR_TYPE, "param not string");
    }
    auto portName = JString::acquire(*args_p);
    auto ret = iflyosInterface::IFLYOS_InterfaceInit(*portName
        , std::vector<std::pair<std::string, iflyosInterface::IFLYOS_InterfaceEvtHandler>>());
    if (ret) {
        return JHelper::unmanagedError(JERRY_ERROR_COMMON, std::to_string(ret));
    }
    return JHelper::unmanagedUndefined();
}

static void after_work_cb(uv_work_t* req,int){
    auto lines = static_cast<std::vector<std::string>*>(req->data);
    delete lines;
    delete req;
}

static void work_cb(uv_work_t*req){
    auto lines = static_cast<std::vector<std::string>*>(req->data);
    auto global = JHelper::create(jerry_get_global_object());
    auto iflyos = global->getProperty("iflyos");
    auto jsListeners = iflyos->getProperty("_listeners");
    auto jsHandler = jsListeners->getProperty((*lines)[0]);
    jsHandler->invoke(*lines);
}

static void msgCallback(const char *evt, const char *msg){
    auto uv = uv_default_loop();
    auto req = new uv_work_t;
    auto lines = new std::vector<std::string>;
    lines->emplace_back(evt);
    iflyosInterface::IFLYOS_InterfaceGetLines(msg, *lines);
    req->data = lines;
    uv_queue_work(uv, req, work_cb, after_work_cb);
}

static jerry_value_t registerMessageHandler(const jerry_value_t func_value,
                                            const jerry_value_t this_value,
                                            const jerry_value_t *args_p,
                                            const jerry_length_t args_cnt)
{
    if (args_cnt != 2) {
        return JHelper::unmanagedError(JERRY_ERROR_RANGE, "param count not 2");
    }
    auto evtName = JString::acquire(args_p[0]);
    auto handler = JHelper::acquire(args_p[1]);
    auto thiz = JHelper::acquire(this_value);
    auto jsListeners = thiz->getProperty("_listeners");
    jsListeners->setProperty(evtName->str(), *handler);
    if(jerry_value_is_null(args_p[1])){
        iflyosInterface::IFLYOS_InterfaceRegisterHandler(nullptr, *evtName);
    }else {
        iflyosInterface::IFLYOS_InterfaceRegisterHandler(msgCallback, *evtName);
    }
    return JHelper::unmanagedUndefined();
}

static jerry_value_t sendAction(const jerry_value_t func_value,
                                const jerry_value_t this_value,
                                const jerry_value_t *args_p,
                                const jerry_length_t args_cnt)
{
    if (args_cnt == 0){
        return JHelper::unmanagedError(JERRY_ERROR_RANGE, "no event argument");
    }
    std::stringstream ss;
    auto evt = JString::acquire(*args_p);
    auto msgCnt = args_cnt - 1;
    auto msgPtr = &args_p[1];
    auto i = 0u;
    for(; i < msgCnt - 1; i++){
        ss << JString::acquire(msgPtr[i])->str() << "\n";
    }
    if(i < msgCnt) {
        ss << JString::acquire(msgPtr[i])->str();
    }
    iflyosInterface::IFLYOS_InterfaceSendEvent(*evt, ss.str().c_str());
    return JHelper::unmanagedUndefined();
}

void initIFLYOSModule(){
    auto iflyos = JHelper::create(jerry_create_object());
    iflyos->setProperty("initGlobalIPCPort", initGlobalIPCPort);
    iflyos->setProperty("sendMsg", sendAction);
    iflyos->setProperty("addListener", registerMessageHandler);
    iflyos->setProperty("_listeners", *JHelper::create(jerry_create_object()));
    auto global = JHelper::create(jerry_get_global_object());
    global->setProperty("iflyos", *iflyos);
}
