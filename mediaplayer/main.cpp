#include <unistd.h>
#include <assert.h>
#include <string>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <atomic>
#include <csignal>
#include <sys/prctl.h>
#include <iostream>

extern "C" {
#include <libavutil/log.h>
}

#include "iFLYOS_IPC.h"
#include "librplayer/include/MediaPlayer.h"
#include "librplayer/include/WavPlayer.h"

#define LOG_TAG "main_exe"

#include "rklog/include/rklog/RKLog.h"
using namespace iflyos::ipc;

static std::unordered_map<std::string, std::shared_ptr<MediaPlayer>> mediaPlayers;
/**
 * map
 * SpeakMediaPlayer AudioMediaPlayer NotificationsMediaPlayer BluetoothMediaPlayer RingtoneMediaPlayer AlertsMediaPlayer
 * to
 * "audio", "tts", "ring", "voiceCall", "alarm", "playback", "system"
 */


static const std::string snSpeakMediaPlayer{"SpeakMediaPlayer"}, snAudioMediaPlayer{
    "AudioMediaPlayer"}, snNotificationsMediaPlayer{"NotificationsMediaPlayer"}, snBluetoothMediaPlayer{
    "BluetoothMediaPlayer"}, snRingtoneMediaPlayer{"RingtoneMediaPlayer"}, snAlertsMediaPlayer{"AlertsMediaPlayer"};

static const std::string snTTS{"tts"}, snAudio{"audio"}, snSystem{"system"}, snRing{"ring"}, snPlayback{
    "playback"}, snAlarm{"alarm"};


static auto playerNameMap = std::unordered_map<std::string, std::string>{{snSpeakMediaPlayer,         snTTS},
                                                                         {snAudioMediaPlayer,         snAudio},
                                                                         {snNotificationsMediaPlayer, snSystem},
                                                                         {snBluetoothMediaPlayer,     snPlayback},//all right?
                                                                         {snRingtoneMediaPlayer,      snRing},
                                                                         {snAlertsMediaPlayer,        snAlarm}};

static std::shared_ptr<MediaPlayer> getPlayer(const std::string &name) {
    if (name == "") {
        throw  std::string("empty player name");
    }
    auto i = mediaPlayers.find(name);
    if (i == mediaPlayers.end()) {
        throw std::string("unknown player name:") + name;
    }
    return i->second;
}

static std::atomic_long sID{0};

class MyMediaPlayerListener : public MediaPlayerListener {
public:
    MyMediaPlayerListener(MediaPlayer *player, const std::string &name) : m_player{player}, mName{name} {

    }

    void notify(long sid, int what, int p1, int p2, int fromThread) override {
        RKLogi("%s has status %d with source %d", mName.c_str(), what, sid);
        switch (what) {
            case MEDIA_PLAYING_STATUS:
            case MEDIA_PLAYING:
                sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {mName, std::to_string(sid), "play"});
                break;
            case MEDIA_PLAYBACK_COMPLETE:
                sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {mName, std::to_string(sid), "finish"});
                break;
            case MEDIA_PAUSE://never called
                sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {mName, std::to_string(sid), "pause"});
                break;
            case MEDIA_STOPED:
                sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {mName, std::to_string(sid), "stop"});
                break;
            case MEDIA_ERROR:
                sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {mName, std::to_string(sid), "error"});
                m_player->stopAsync();
                break;
        }
    }

private:
    MediaPlayer *m_player;
    const std::string mName;
};

static void onCreate(const std::string &evtName, const std::vector<std::shared_ptr<std::string>> &params) {
    auto name = *params[0];

//  const char *tag = nullptr;
    auto name2name = playerNameMap.find(name);
    if (name2name == playerNameMap.end()) {
        RKLogw("not find player %s, use null tag\n", name.c_str());
    }
//    else {
//        tag = name2name->second.c_str();
//    }
    auto player = std::make_shared<MediaPlayer>(/*tag*/);
    player->setListener(new MyMediaPlayerListener(player.get(), name));
    mediaPlayers[name] = player;
}


//https://stackoverflow.com/questions/37093920/function-to-generate-a-tuple-given-a-size-n-and-a-type-t

//template<size_t N>
//class P {
//public:
//    static auto fetch(const char *msg) {
//        return _getParams(msg, std::make_index_sequence<N>{});
//    }
//
//    template<size_t I>
//    std::string &get() {
//        return std::get<I>(m_params);
//    }
//
//    P(const std::vector<std::shared_ptr<std::string>> &params) : m_params{fetch(msg)} {
//    }
//
//private:
//    template<size_t... Is>
//    static auto _getParams(const char *msg, std::index_sequence<Is...>) {
//        std::vector<std::string> lines;
//        iflyosInterface::IFLYOS_InterfaceGetLines(msg, lines);
//        assert(lines.size() == sizeof...(Is));
//        return std::make_tuple(lines[Is]...);
//    }
//
//    template<typename T, size_t... Is> using TypeWithIndices = T;
//
//    decltype(P::fetch(nullptr)) m_params;
//};

//#define DECLARE_PN(args...) \
//enum class _PN{args,count}; \
//auto _p = P<static_cast<int>(_PN::count)>(msg); \
//auto player = getPlayer(GETP(name)) \
//
//#define GETP(n) _p.get<static_cast<int>(_PN::n)>()

#define DECLARE_PN(args...) \
enum class _PN{args, count}; \
auto player = getPlayer(GETP(name)) \

#define GETP(n) (*params[static_cast<int>(_PN::n)])

static void onSetSource(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    long sid = ++sID;//atomic overrides ++
    DECLARE_PN(name, sourceType, path, repeatOffsetUnion);
#if defined PCM_TTS
    if(snTTS == player->getTag() && GETP(sourceType) == "PCM"){
        pcmPlayer->setDataSource(GETP(path));
    } else
#endif
    {
        if (GETP(sourceType) == "MP3") {
            player->setDataSource(std::string("file://") + GETP(path), sid);
            player->setLooping(GETP(repeatOffsetUnion) == "true" ? 1 : 0);
        } else {
            player->setDataSource(GETP(path), sid);
            auto offset = std::atoi(GETP(repeatOffsetUnion).c_str());
            if (offset > 0) {
                player->seekTo(offset);
            }
        }
        auto status = player->prepareAsync();
        if (status) {
            RKLoge("player %s prepareAsync() returns %d", GETP(name).c_str(), status);
        }
    }
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_SOURCE_ID", {GETP(name), std::to_string(sid)});

};

static void onReportOffset(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, sid);
    auto ms = player->getCurrentPosition();
    if ( ms < 0) ms = 0;
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_OFFSET", {GETP(name), GETP(sid), std::to_string(ms)});
}

static void onPlay(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, sid);
    auto r = false;
    if(!player->isPlaying() && player->getSourceId() != 0) {
        player->start();
        r = true;
    }else{
        RKLogi("player %s play failed because is playing", GETP(name).c_str());
    }
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_CONTROL_RESULT", {GETP(name), GETP(sid), "play", r ? "true" : "false"});

}

static void onStop(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, sid);
    auto r = player->stopAsync();
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_CONTROL_RESULT", {GETP(name), GETP(sid), "stop", r == 0 ? "true" : "false"});
}

static void onResume(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, sid);
    auto r = false;
    if(!player->isPlaying()) {
        player->resume();
        r = true;
    } else {
        RKLogi("player %s resume failed because is playing", GETP(name).c_str());
    }
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_CONTROL_RESULT", {GETP(name), GETP(sid), "resume", r ? "true" : "false"});
    if (r) {
        //resume() not trigger playing staus notify
        sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {GETP(name), GETP(sid), "play"});
    }
}

static void onPause(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, sid);
    auto r = false;
    if(player->isPlaying()) {
        player->pause();
        r = true;
    } else {
        RKLogi("player %s pause failed because is not playing", GETP(name).c_str());
    }
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_CONTROL_RESULT", {GETP(name), GETP(sid), "pause", r ? "true" : "false"});
    if (r) {
        sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_PLAY_STATUS", {GETP(name), GETP(sid), "pause"});
    }
}

static void onReportVolume(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name);
    auto v = player->getVolume();
    auto mute = player->isMute();
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_VOLUME", {GETP(name), std::to_string(v), mute ? "true" : "false"});
}

static void onSetMute(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, mute);
    player->setMute(GETP(mute) == "true");
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_VOLUME", {GETP(name), "50", "false"});
}

static void onSetVolume(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name, vol);
    auto volStr = GETP(vol);
    std::cout << "try to set new vol " << volStr << std::endl;
    auto volI = std::atoi(volStr.c_str());
    if (volStr.length() > 1 && (volStr[0] == '-' || volStr[0] == '+')) {
        player->adjuestVolume(volI);
    } else {
        player->setVolume(volI);
    }
    volStr = std::to_string(player->getVolume());
    sendMessage(ROUTER_PORT_NAME, "DO_MEDIAPLAYER_REPORT_VOLUME", {GETP(name), volStr, player->isMute() ? "true" : "false"});
}

static void onRelease(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    DECLARE_PN(name);
    player->stop();
    player->reset();
    mediaPlayers.erase(GETP(name));
}
static void onReleaseAll(const std::string &name, const std::vector<std::shared_ptr<std::string>> &params) {
    for (auto i : mediaPlayers) {
        i.second->stop();
        i.second->reset();
    }

    mediaPlayers.clear();
}


int main(int argc, const char *argv[]) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    //av_log_set_level(48);
    auto ret = initIPCPort(MP_PORT_NAME, [](const char *msg){
        printf("%s\n", msg);
    });
    if (ret) return ret;
    registerMessageHandler("EVT_MEDIAPLAYER_CREATE", &onCreate);
    registerMessageHandler("EVT_MEDIAPLAYER_SET_SOURCE", &onSetSource);
    registerMessageHandler("EVT_MEDIAPLAYER_REPORT_OFFSET", &onReportOffset);
    registerMessageHandler("EVT_MEDIAPLAYER_PLAY", &onPlay);
    registerMessageHandler("EVT_MEDIAPLAYER_STOP", &onStop);
    registerMessageHandler("EVT_MEDIAPLAYER_RESUME", &onResume);
    registerMessageHandler("EVT_MEDIAPLAYER_PAUSE", &onPause);
    registerMessageHandler("EVT_MEDIAPLAYER_REPORT_VOLUME", &onReportVolume);
    registerMessageHandler("EVT_MEDIAPLAYER_SET_MUTE", &onSetMute);
    registerMessageHandler("EVT_MEDIAPLAYER_SET_VOLUME", &onSetVolume);
    registerMessageHandler("EVT_MEDIAPLAYER_RELEASE", &onRelease);
    registerMessageHandler("EVT_STOPPED", &onReleaseAll);

//    //test
//    onCreate(nullptr, "AudioMediaPlayer");
//    onReportVolume(nullptr, "AudioMediaPlayer");
//    return 0;

    while (true) {
        sleep(10);
    }
}