//
// Created by jan on 18-11-26.
//


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <mutex>
#include <map>
#include <iostream>
#include <vector>
#include <sstream>
#include <iterator>
#include <set>
#include <algorithm>
#include <cstring>

#include "IFLYOSInterfaceInternal.h"
#include "IFLYOSInterface.h"

namespace iflyosInterface {

static int sock = -1;
static std::map<std::string, IFLYOS_InterfaceEvtHandler> handlers;
static std::map<std::string, std::set<std::string>> receivers;
static std::thread *receiver;
static volatile bool quit;
static std::mutex* handlerTableMutex;
static struct sockaddr_un localAddr;
static struct sockaddr_un osAddr;
static struct sockaddr_un umbrellaAddr;
static std::string osAddrStr(IFLYOS_EVENT_RECV_ADDR);
static std::string umbrellaAddrStr(IFLYOS_UMBRELLA_RECV_ADDR);

static void IFLYOS_InterfaceRegisteration(const char * /*evt*/, const char *msg) {
    auto str = std::string(msg);
    auto splitPos = str.find(' ');
    if (splitPos == std::string::npos) {
        printf("%s can not find ' ' in message:%s\n", &localAddr.sun_path[1], msg);
        return;
    }
    //std::cout << "msg:" << str << "pos:" << splitPos << std::endl;

    auto intfVerStr = str.substr(0, splitPos);
    auto intfVer = std::atoi(intfVerStr.c_str());
    if(intfVer != IFLYOS_INTERFACE_CURRENT_VERSION){
        printf("error: IPC interface version not match. %d <> %d\n", intfVer, IFLYOS_INTERFACE_CURRENT_VERSION);
        return;
    }

    str = str.substr(splitPos + 1);
    splitPos = str.find(' ');
    if (splitPos == std::string::npos) {
        printf("%s can not find ' ' in message:%s\n", &localAddr.sun_path[1], msg);
        return;
    }

    auto addr = str.substr(0, splitPos);
    auto events = str.substr(splitPos + 1, str.length() - splitPos);

    std::istringstream iss(events);
    std::vector<std::string> eventVector(std::istream_iterator<std::string>{iss},
                                         std::istream_iterator<std::string>());
    {
        std::lock_guard<std::mutex> lock(*handlerTableMutex);
        for (auto const &event : eventVector) {
            std::cout << std::string(&localAddr.sun_path[1]) << " register event " << event << " to address " << addr << std::endl;
            receivers[event].insert(addr);
        }
    }
}

static void IFLYOS_InterfaceReceiverWorker() {
    char buf[4096];
    while (!quit) {
        auto rd = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (rd <= 0) {
            printf("%s receive message failed:%zd, errno:%d\n", &localAddr.sun_path[1], rd, (int)errno);
            break;
        }
        //printf("IFLYOS_Interface receive message length:%ld\n", rd);

        buf[rd] = 0;
        auto str = std::string(buf);
        auto splitPos = str.find(' ');
        if (splitPos == std::string::npos) {
            printf("%s can not find ' ' in message:%s\n", &localAddr.sun_path[1], buf);
            continue;
        }

        auto evt = str.substr(0, splitPos);
        auto msg = str.substr(splitPos + 1);

        auto handlerValue = handlers.end();
        {
            std::lock_guard<std::mutex> lock(*handlerTableMutex);
            handlerValue = handlers.find(evt);
            if (handlerValue == handlers.end()) {
                std::cout << std::string(&localAddr.sun_path[1]) << " can not find handler for event:" << evt << std::endl;
                continue;
            }

            std::cout << std::string(&localAddr.sun_path[1]) << " to process evt:" << evt << ", msg:" << msg << std::endl;
        }
        handlerValue->second(evt.c_str(), msg.c_str());
    }
}

static void IFLYOS_InterfaceUmbrellaReceiverWorker() {
    char buf[4096];
    while (!quit) {
        auto rd = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (rd <= 0) {
            printf("%s receive message failed:%zd, errno:%d\n", &localAddr.sun_path[1], rd, (int)errno);
            break;
        }
        //printf("IFLYOS_Interface receive message length:%ld\n", rd);

        buf[rd] = 0;
        auto str = std::string(buf);
        auto splitPos = str.find(' ');
        if (splitPos == std::string::npos) {
            printf("%s can not find ' ' in message:%s\n", &localAddr.sun_path[1], buf);
            continue;
        }

        auto evt = str.substr(0, splitPos);
        auto msg = str.substr(splitPos + 1);

        auto handlerValue = handlers.end();
        IFLYOS_InterfaceEvtHandler handlerFun;
        {
            std::lock_guard<std::mutex> lock(*handlerTableMutex);
            handlerValue = handlers.find(evt);
            handlerFun = handlerValue == handlers.end() ? nullptr : handlerValue->second;
        }

        if (!handlerFun) {
            //if not handled by umbrella, do forwarding
            IFLYOS_InterfaceUmbrellaSendEvent(evt.c_str(), msg.c_str());
            continue;
        }

        std::cout << std::string(&localAddr.sun_path[1]) << " to process evt:" << evt << ", msg:" << msg << std::endl;

        handlerFun(evt.c_str(), msg.c_str());
    }
}

static int IFLYOS_InterfaceBindEvents(const char *address, const char *events) {
    std::stringstream ss;
    ss << IFLYOS_INTERFACE_CURRENT_VERSION << " " << address << " " << events;
    return IFLYOS_InterfaceSendEvent(IFLYOS_DO_REGISTER_EVT_RECEIVER, ss.str().c_str());
}

int IFLYOS_InterfaceOSInit(){
    const std::vector<std::pair<std::string, IFLYOS_InterfaceEvtHandler>> empty;
    return IFLYOS_InterfaceInit(IFLYOS_EVENT_RECV_ADDR, empty);
}

int IFLYOS_InterfaceUmberllaInit() {
    const std::vector<std::pair<std::string, IFLYOS_InterfaceEvtHandler>> empty;
    auto ret = IFLYOS_InterfaceInit(IFLYOS_UMBRELLA_RECV_ADDR, empty);
    if(ret) return ret;
    handlers[IFLYOS_DO_REGISTER_EVT_RECEIVER] = IFLYOS_InterfaceRegisteration;

    return 0;
}

int IFLYOS_InterfaceInit(const char *address
        , const std::vector<std::pair<std::string, IFLYOS_InterfaceEvtHandler>>& eventsRegistration) {
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    handlerTableMutex = new std::mutex();

    int bufSize = 4096;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));


    localAddr.sun_family = AF_UNIX;
    memset(localAddr.sun_path, 0, sizeof(localAddr.sun_path));
    strcpy(&localAddr.sun_path[1], address);

    osAddr.sun_family = AF_UNIX;
    memset(osAddr.sun_path, 0, sizeof(osAddr.sun_path));
    strcpy(&osAddr.sun_path[1], IFLYOS_EVENT_RECV_ADDR);

    umbrellaAddr.sun_family = AF_UNIX;
    memset(umbrellaAddr.sun_path, 0, sizeof(umbrellaAddr.sun_path));
    strcpy(&umbrellaAddr.sun_path[1], IFLYOS_UMBRELLA_RECV_ADDR);


    /* Bind the UNIX domain address to the created socket */
    if (bind(sock, (struct sockaddr *) &localAddr, sizeof(struct sockaddr_un))) {
        return -2;
    }

    quit = false;
    if(umbrellaAddrStr == address){
        receiver = new std::thread(&IFLYOS_InterfaceUmbrellaReceiverWorker);
    }else {
        receiver = new std::thread(&IFLYOS_InterfaceReceiverWorker);
    }
    if(eventsRegistration.size() > 0) {
        auto eventsStr = std::string();
        for (auto &reg : eventsRegistration) {
            eventsStr += " " + reg.first;
            IFLYOS_InterfaceRegisterHandler(reg.second, reg.first.c_str());
        }
        eventsStr.erase(eventsStr.begin(), std::find_if(eventsStr.begin(), eventsStr.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        return IFLYOS_InterfaceBindEvents(address, eventsStr.c_str());
    }

    return 0;
}

void IFLYOS_InterfaceDeinit() {
    if (sock < 0) {
        return;
    }
    delete handlerTableMutex;
    shutdown(sock, SHUT_RDWR);
    quit = true;
    receiver->join();
    handlers.clear();
}

void IFLYOS_InterfaceUmberllaClearForChild() {
    if (sock < 0) {
        return;
    }
    delete handlerTableMutex;
    close(sock);
}

int IFLYOS_InterfaceRegisterHandler(const IFLYOS_InterfaceEvtHandler handler, const char *evt) {
    std::lock_guard<std::mutex> lock(*handlerTableMutex);
    if(handler == nullptr) {
        handlers.erase(evt);
    }else{
        handlers[evt] = handler;
    }
    return 0;
}

int IFLYOS_InterfaceSendEventStr(const char *evt, const std::string& msg) {
    return IFLYOS_InterfaceSendEvent(evt, msg.c_str());
}

int IFLYOS_InterfaceSendEvent(const char *evt, const char *msg) {
    printf("%s to send evt:%s, msg: %s\n", &localAddr.sun_path[1], evt, msg);
    auto evtLen = strlen(evt);
    auto msgLen = strlen(msg);
    if (evtLen + msgLen > 4096) {
        printf("%s write message too long\n", &localAddr.sun_path[1]);
        return -1;
    }
    auto str = std::string() + evt + " " + msg;
    auto name = &umbrellaAddr;
    auto wr = sendto(sock, str.c_str(), evtLen + msgLen + 1, 0, (const struct sockaddr *) name,
                     sizeof(struct sockaddr_un));
    if (wr <= 0) {
        printf("%s write message failed:%zd, errno:%d\n", &localAddr.sun_path[1], wr, (int)errno);
        return -errno;
    }

    return 0;
}

int IFLYOS_InterfaceOSSendEvent(const char *evt, const char *msg) {
    printf("IFLYOS_Interface to send evt:%s, msg: %s\n", evt, msg);
    auto evtLen = strlen(evt);
    auto msgLen = strlen(msg);
    if (evtLen + msgLen > 4096) {
        printf("%s write message too long\n", &localAddr.sun_path[1]);
        return -1;
    }
    auto str = std::string() + evt + " " + msg;
    auto name = &umbrellaAddr;
    auto wr = sendto(sock, str.c_str(), evtLen + msgLen + 1, 0, (const struct sockaddr *) name,
                     sizeof(struct sockaddr_un));
    if (wr <= 0) {
        printf("%s write message failed:%zd, errno:%d\n", &localAddr.sun_path[1], wr, (int)errno);
        return -errno;
    }

    return 0;
}

//int IFLYOS_InterfaceUmbrellaSendAction(const char *evt, const char *msg) {
//    auto evtLen = strlen(evt);
//    auto msgLen = strlen(msg);
//    if (evtLen + msgLen > 4096) {
//        printf("%s write message too long\n", &localAddr.sun_path[1]);
//        return -1;
//    }
//    auto str = std::string() + evt + " " + msg;
//    auto name = &osAddr;
//#ifdef LOG_MSG
//    auto msgStr = std::string(evt) + "|"+msg;
//    std::replace(msgStr.begin(), msgStr.end(), '\n', '|');
//    msgStr.erase(std::find_if(msgStr.rbegin(), msgStr.rend(), [](int ch) {
//        return !(ch=='|');
//    }).base(), msgStr.end());
//    std::cout << "APP->IFLYOS:" <<  msgStr << std::endl;
//#endif
//    auto wr = sendto(sock, str.c_str(), evtLen + msgLen + 1, 0, (const struct sockaddr *) name,
//                     sizeof(struct sockaddr_un));
//    if (wr <= 0) {
//        printf("%s write message failed:%zd, errno:%d\n", &localAddr.sun_path[1], wr, (int)errno);
//        return -errno;
//    }
//
//    return 0;
//}

int IFLYOS_InterfaceUmbrellaSendEvent(const char *evt, const char *msg) {
    printf("%s send evt:%s, msg: %s\n", &localAddr.sun_path[1], evt, msg);
    struct sockaddr_un addr;
    auto addresses = receivers.end();
    {
        std::lock_guard<std::mutex> lock(*handlerTableMutex);
        addresses = receivers.find(evt);
        if (addresses == receivers.end()) {
            std::cout << std::string(&localAddr.sun_path[1]) << " can not find receiver address for event:" << evt << std::endl;
            return - 1;
        }
    }

    addr.sun_family = AF_UNIX;
    auto evtLen = strlen(evt);
    auto msgLen = strlen(msg);
    if (evtLen + msgLen > 4096) {
        printf("%s write message too long\n", &localAddr.sun_path[1]);
        return -2 ;
    }
    auto str = std::string() + evt + " " + msg;

    long allOk = true;

#ifdef LOG_MSG
    auto msgStr = std::string(evt) + "|"+msg;
        std::replace(msgStr.begin(), msgStr.end(), '\n', '|');
        msgStr.erase(std::find_if(msgStr.rbegin(), msgStr.rend(), [](int ch) {
            return !(ch=='|');
        }).base(), msgStr.end());
        std::cout << "IFLYOS->APP:" << msgStr << std::endl;
#endif

    for(auto &address : addresses->second) {
        memset(addr.sun_path, 0, sizeof(addr.sun_path));
        strcpy(&addr.sun_path[1], address.c_str());
        auto wr = sendto(sock, str.c_str(), evtLen + msgLen + 1, 0, (const struct sockaddr *) &addr,
                         sizeof(struct sockaddr_un));

        if (wr <= 0) {
            printf("%s write message failed:%zd, errno:%d\n", &localAddr.sun_path[1], wr, (int)errno);
            allOk = false;
        }
    }

    return allOk ? 0 : -1;
}

void IFLYOS_InterfaceGetLines(const std::string &strVal, std::vector<std::string> &vecStr) {
    size_t nLast = 0;
    size_t nIndex = strVal.find_first_of('\n', nLast);
    while (nIndex != std::string::npos) {
        vecStr.push_back(strVal.substr(nLast, nIndex - nLast));
        nLast = nIndex + 1;
        nIndex = strVal.find_first_of('\n', nLast);
    }

    if (nIndex - nLast > 0)
        vecStr.push_back(strVal.substr(nLast, nIndex - nLast));
}

int getCurrentVersion(){
    return IFLYOS_INTERFACE_CURRENT_VERSION;
}

}
