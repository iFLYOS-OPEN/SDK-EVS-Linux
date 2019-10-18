#ifndef IOTJS_MODULE_IFLYOS_WPACONTROLLER_H
#define IOTJS_MODULE_IFLYOS_WPACONTROLLER_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "wpa_ctrl.h"

namespace iflyos {

typedef struct {
    int nid;
    std::string info;
} NetworkInfo;

class WpaController {
public:
    static WpaController & instance();
    ~WpaController() = default;

    bool hasWifi();
    bool clearWifi();
    bool configWifi(const std::string &ssid, const std::string &password);

private:
    WpaController();

    bool newWpaCtrl(bool attach);
    void deleteWpaCtrl();

    bool listNetwork(std::vector<NetworkInfo> &list);
    bool addNetwork(int &nid);
    bool setNetwork(int nid, const std::string &ssid, const std::string &password);
    bool selectNetwork(int nid);
    bool enableNetwork(int nid);
    bool removeNetwork(int nid);
    bool clearNetwork();
    bool saveConfig();

    bool execute(const std::string &cmd, std::string &reply);
    void recvThread();

private:
    wpa_ctrl *m_wpaCtrl;
    bool m_attach;

    std::mutex m_mutexBus;
    std::mutex m_mutexMsg;
    std::condition_variable m_cond;
    std::string m_result;

    std::thread m_recvThread;
    bool m_recvStop;
};
}

#endif // IOTJS_MODULE_IFLYOS_WPACONTROLLER_H