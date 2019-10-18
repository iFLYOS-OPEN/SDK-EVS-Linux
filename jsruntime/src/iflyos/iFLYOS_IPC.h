#ifndef IFLYOSCLIENTSDK_IPC_H
#define IFLYOSCLIENTSDK_IPC_H

#ifndef __cplusplus
#error "iflyos ipc interface only support c++ language"
#endif

#include <functional>
#include <vector>

#define ROUTER_PORT_NAME "router"

namespace iflyos{
namespace ipc {

#define MESSAGE_MAX (4096)
#define PARAM_MAX (1024)

typedef std::function<void(const std::string &name
    , const std::vector<std::shared_ptr<std::string>> &params)> Handler;
//create one global(Operating System wide) IPC port, only one port allow in one process
bool initIPCPort(const std::string &portName, std::function<void(const char *msg)> logger);

bool sendMessage(const std::string& dest, const std::string &name
    , const std::initializer_list<const std::string> params);

bool sendMessage2Router(const std::string &name, const std::initializer_list<const std::string> params);

bool registerMessageHandler(const std::string& name, const Handler& handler);
}
}

#endif//#ifndef IFLYOSCLIENTSDK_IPC_H