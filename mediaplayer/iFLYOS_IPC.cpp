#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <sstream>

#include "iFLYOS_IPC.h"

static int sock = -1;
static std::map<std::string, iflyos::ipc::Handler> handlers;
static std::thread *receiver;
static std::mutex* handlerTableMutex;
static struct sockaddr_un localAddr;
static struct sockaddr_un routerAddr;
static std::string routerAddrStr(ROUTER_PORT_NAME);
static std::function<void(const char *msg)> logger;

#define LOG_MAX (256)

static void flog(const char* format, ...) {
    char buf[LOG_MAX];
    va_list vl;
    va_start(vl, format);
    vsnprintf(buf, sizeof(buf), format, vl);
    va_end(vl);
    logger(buf);
}

static void receiverThreadEntry() {
    char buf[MESSAGE_MAX];

    while (true) {
        auto rd = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (rd <= 0) {
            flog("%s receive message failed:%zd, errno:%d\n", &localAddr.sun_path[1], rd, (int) errno);
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
            flog("%s drop message, can not find ' ' in message:%s\n", &localAddr.sun_path[1], buf);
            continue;
        }
        std::string name(buf, idx);
        idx++;
        auto paramCount = 0;
        std::vector<std::shared_ptr<std::string>> params;
        //params.clear();
        while(idx < rd){
            int16_t * lenP = (int16_t *)&buf[idx];
            auto len = *lenP;
            if(len > PARAM_MAX){
                flog("%s drop message, %d param too long in message:%s\n"
                    , &localAddr.sun_path[1], paramCount, name.c_str());
                break;
            }
            idx += sizeof(uint16_t);

            if(idx + len > rd) {
                flog("%s drop message, %d param overflow in message:%s\n"
                    , &localAddr.sun_path[1], paramCount, name.c_str());
                break;
            }

            params.emplace_back(std::shared_ptr<std::string>(new std::string(&buf[idx], len)));
            idx += len;
            paramCount++;
        }
        if(idx != rd){//has some error, not consume out buf
            continue;
        }
        auto handlerValue = handlers.end();
        {
            std::lock_guard<std::mutex> lock(*handlerTableMutex);
            handlerValue = handlers.find(name);
            if (handlerValue == handlers.end()) {
                flog("%s can not find handler for message %s", &localAddr.sun_path[1], name.c_str());
                continue;
            }

            flog("%s to process message %s", &localAddr.sun_path[1], name.c_str());
        }
        handlerValue->second(name, params);
    }
}

bool iflyos::ipc::initIPCPort(const std::string &portName, std::function<void(const char *msg)> l) {
    logger = l;
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        logger("create socket FD failed");
        return -1;
    }
    handlerTableMutex = new std::mutex();

    int bufSize = MESSAGE_MAX;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));


    localAddr.sun_family = AF_UNIX;
    memset(localAddr.sun_path, 0, sizeof(localAddr.sun_path));
    strcpy(&localAddr.sun_path[1], portName.c_str());

    routerAddr.sun_family = AF_UNIX;
    memset(routerAddr.sun_path, 0, sizeof(routerAddr.sun_path));
    strcpy(&routerAddr.sun_path[1], ROUTER_PORT_NAME);


    /* Bind the UNIX domain address to the created socket */
    if (bind(sock, (struct sockaddr *) &localAddr, sizeof(struct sockaddr_un))) {
        logger("bind socket address failed");
        return -2;
    }

    receiver = new std::thread(&receiverThreadEntry);
    receiver->detach();//not need deinit now
    return 0;
}

bool iflyos::ipc::sendMessage2Router(const std::string &name, const std::initializer_list<const std::string> params) {
    return iflyos::ipc::sendMessage(routerAddrStr, name, params);
}

bool iflyos::ipc::sendMessage(const std::string& dest, const std::string &name
    , const std::initializer_list<const std::string> params) {
    auto nameCStr = name.c_str();

    if(name.length() >= MESSAGE_MAX - 1){
        flog("%s send message %s failed for name too long %d <> %d"
            , &localAddr.sun_path[1], nameCStr, name.length(), MESSAGE_MAX);
        return false;
    }

    char buf[MESSAGE_MAX];
    std::stringstream logmsg;
    logmsg << name;

    int idx = 0;
    memcpy(&buf[idx], nameCStr, name.length());
    idx += name.length();

    buf[idx] = ' ';
    idx++;

    logmsg << " ";
    for(auto p : params){
        idx += p.length() + sizeof(uint16_t);//'int16 length header'
        if(idx  >= (int)sizeof(buf)){
            flog("%s send message %s failed for message too long", &localAddr.sun_path[1], nameCStr);
            return false;
        }

        auto lenP = (int16_t *)&buf[idx - p.length() - sizeof(uint16_t)];
        *lenP = p.length();

        memcpy(&buf[idx - p.length()], p.c_str(), p.length());
        logmsg << " | " << p;
    }
    flog("%s to send message to %s : %s", &localAddr.sun_path[1], dest.c_str(), logmsg.str().c_str());

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memset(addr.sun_path, 0, sizeof(addr.sun_path));
    strcpy(&addr.sun_path[1], dest.c_str());

    auto wr = sendto(sock, buf, idx, 0, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
    if (wr <= 0) {
        printf("%s write message failed:%zd, errno:%d\n", &localAddr.sun_path[1], wr, (int)errno);
        return false;
    }

    return true;
}

bool iflyos::ipc::registerMessageHandler(const std::string &name, const Handler& handler) {
    auto msg = (handler == nullptr ? "DO_UNREGISTER_EVT_RECEIVER" : "DO_REGISTER_EVT_RECEIVER");
    if(!sendMessage2Router(msg, {name, &localAddr.sun_path[1]})){
        return false;
    }
    {
        std::lock_guard <std::mutex> lock(*handlerTableMutex);
        if (handler == nullptr) {
            handlers.erase(name);
        } else {
            handlers[name] = handler;
        }
    }
    return true;
}
