//
// Created by Shawn on 2019/3/7.
//

#include "iotjs_module_iFLYOS_wpacontroller.h"
#include <algorithm>
#include <sstream>
#include <functional>
#include <iostream>
#include <string.h>
#include <unistd.h>

namespace iflyos {

#define CTRL_PATH   "/var/run/wpa_supplicant/wlan0"

WpaController::WpaController() : m_wpaCtrl(nullptr), m_attach(false), m_recvStop(true) {
}

WpaController & WpaController::instance() {
    static WpaController wpaCtrl;
    return wpaCtrl;
}

bool WpaController::hasWifi() {
    std::lock_guard<std::mutex> lock(m_mutexBus);
    if (!newWpaCtrl(false)) {
        return false;
    }

    std::vector<NetworkInfo> wifiList;
    listNetwork(wifiList);
    deleteWpaCtrl();

    auto compFunc = [](const NetworkInfo &wifiInfo) {
        return wifiInfo.info.find("[DISABLED]") == std::string::npos;
    };
    return std::find_if(wifiList.begin(), wifiList.end(), compFunc) != wifiList.end();
}

bool WpaController::configWifi(const std::string &ssid, const std::string &password) {
    std::lock_guard<std::mutex> lock(m_mutexBus);
    if (!newWpaCtrl(true)) {
        return false;
    }

    int nid = -1;
    if (!clearNetwork() || !addNetwork(nid) || !setNetwork(nid, ssid, password) || !selectNetwork(nid)) {
        deleteWpaCtrl();
        return false;
    }

    m_result.clear();
    m_recvStop = false;
    m_recvThread = std::thread(std::bind(&WpaController::recvThread, this));

    std::unique_lock<std::mutex> msglock(m_mutexMsg);
    m_cond.wait_for(msglock, std::chrono::seconds(30));
    m_recvStop = true;
    m_recvThread.join();

    if (m_result == WPA_EVENT_CONNECTED) {
        enableNetwork(nid);
        saveConfig();
        std::system("udhcpc -q -n -s /usr/share/udhcpc/default.script -i wlan0");
    }

    deleteWpaCtrl();
    return m_result == WPA_EVENT_CONNECTED;
}

bool WpaController::clearWifi() {
    std::lock_guard<std::mutex> lock(m_mutexBus);
    if (!newWpaCtrl(false)) {
        return false;
    }

    bool ret = clearNetwork();
    deleteWpaCtrl();
    return ret;
}

bool WpaController::newWpaCtrl(bool attach) {
    if (m_wpaCtrl) {
        deleteWpaCtrl();
    }
#ifndef NO_WPA_CLIENT
    if (!(m_wpaCtrl = wpa_ctrl_open(CTRL_PATH))) {
        std::cout << "Fail to open wpa control" << std::endl;
        return false;
    }

    if (wpa_ctrl_attach(m_wpaCtrl) != 0) {
        std::cout << "Fail to attach wpa control" << std::endl;
        return false;
    }
#endif
    m_attach = attach;
    return true;
}

void WpaController::deleteWpaCtrl() {
#ifndef NO_WPA_CLIENT
    if (m_wpaCtrl) {
        if (m_attach && wpa_ctrl_detach(m_wpaCtrl) != 0) {
            std::cout << "Fail to detach wpa control" << std::endl;
        }

        wpa_ctrl_close(m_wpaCtrl);
        m_wpaCtrl = nullptr;
        m_attach = false;
    }
#endif
}

bool WpaController::addNetwork(int &nid) {
    std::string reply;
    auto isNotDig = [](char ch) { return isdigit(ch) == 0; };
    if (!execute("ADD_NETWORK", reply) ||
        std::find_if(reply.begin(), reply.end(), isNotDig) != reply.end()) {
        return false;
    }

    nid = std::stoi(reply);
    return true;
}


bool WpaController::listNetwork(std::vector<NetworkInfo> &nidList) {
    std::string reply;
    if (!execute("LIST_NETWORKS", reply) || reply.find("network id") == std::string::npos) {
        return false;
    }

    std::string line;
    std::stringstream sstream(reply);
    std::getline(sstream, line);
    while (sstream.good()) {
        std::getline(sstream, line);
        if (!line.empty()) {
            NetworkInfo netInfo;
            netInfo.nid = std::stoi(line.substr(0, line.find(' ')));
            netInfo.info = line;
            nidList.emplace_back(netInfo);
        }
    }
    return true;
}

bool WpaController::setNetwork(int nid, const std::string &ssid, const std::string &password) {
    std::string setCmd = "SET_NETWORK " + std::to_string(nid);
    std::string setSsidCmd = setCmd + " ssid \"" + ssid + "\"";
    std::string setPskCmd = setCmd + (password.empty() ? " key_mgmt NONE" : " psk \"" + password + "\"");

    std::string replySsid;
    std::string replyPsk;
    return execute(setSsidCmd, replySsid) && replySsid == "OK" &&
           execute(setPskCmd, replyPsk) && replyPsk == "OK";
}

bool WpaController::selectNetwork(int nid) {
    std::string reply;
    std::string selectCmd = "SELECT_NETWORK " + std::to_string(nid);
    return execute(selectCmd, reply) && reply == "OK";
}

bool WpaController::enableNetwork(int nid) {
    std::string reply;
    std::string selectCmd = "ENABLE_NETWORK " + std::to_string(nid);
    return execute(selectCmd, reply) && reply == "OK";
}

bool WpaController::removeNetwork(int nid) {
    std::string reply;
    std::string removeCmd = "REMOVE_NETWORK " + std::to_string(nid);
    return execute(removeCmd, reply) && reply == "OK";
}

bool WpaController::clearNetwork() {
    std::vector<NetworkInfo> wifiList;
    if (!listNetwork(wifiList)) {
        return false;
    }

    for (auto wifiInfo : wifiList) {
        if (!removeNetwork(wifiInfo.nid)) {
            return false;
        }
    }
    return true;
}

bool WpaController::saveConfig() {
    std::string reply;
    return execute("SAVE_CONFIG", reply) && reply == "OK";
}

bool WpaController::execute(const std::string &cmd, std::string &reply) {
    if (!m_wpaCtrl) {
        return false;
    }

    std::cout << "cmd: " << cmd << std::endl;

    char buff[1024] = {0};
    size_t buffLen = sizeof(buff);
#ifndef NO_WPA_CLIENT
    int ret = wpa_ctrl_request(m_wpaCtrl, cmd.c_str(), cmd.length(), buff, &buffLen, nullptr);
#else
    int ret = 0;
#endif
    if (ret == 0) {
        reply.assign(buff, (unsigned long)buffLen - 1);
        std::cout << "result: " << reply << std::endl;
    }
    return ret == 0;
}

void WpaController::recvThread() {
    while (!m_recvStop) {
#ifndef NO_WPA_CLIENT
        int ret = wpa_ctrl_pending(m_wpaCtrl);
#else
        int ret = 0;
#endif
        if (ret == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else if (ret == -1) {
            m_cond.notify_one();
            break;
        }

        char msg[1024] = {0};
        size_t msgLen = sizeof(msg);
        auto compFunc = [&](const char *event) {
            if (!strstr(msg, event)) {
                return false;
            } else {
                m_result = event;
                return true;
            }
        };
#ifndef NO_WPA_CLIENT
        if (wpa_ctrl_recv(m_wpaCtrl, msg, &msgLen) == 0) {
            std::cout << "wpa_client:" << msg << std::endl;
            if (compFunc(WPA_EVENT_CONNECTED) || compFunc(WPA_EVENT_TEMP_DISABLED) ||
                compFunc(WPA_EVENT_AUTH_REJECT) || compFunc(WPA_EVENT_NETWORK_NOT_FOUND)) {
                m_cond.notify_one();
                break;
            }
        }
#endif
    }
}
}
