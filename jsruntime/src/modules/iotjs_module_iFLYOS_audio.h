//
// Created by jan on 19-5-21.
//

#ifndef MESSAGER_IOTJS_MODULE_IFLYOS_AUDIO_H
#define MESSAGER_IOTJS_MODULE_IFLYOS_AUDIO_H

#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <thread>
#include <fstream>
#include "../SDS/InProcessSDS.h"
#ifdef SNOWBOY
#include <snowboy-detect.h>
#endif
#include <alsa/asoundlib.h>

#include "iotjs_module_iFLYOS_threadpool.h"
#include "cae_intf.h"

namespace iflyos{
namespace audio{

enum class Format {
    S16LE,
    S32LE
};

class AudioSink{
public:
    virtual void onAudio(const char* data, const int bytes, const Format format, const int channelNum) = 0;
};

class AudioSource{
public:
    void connectSink(AudioSink* sink);

protected:
    void fireAudio(const char* data, const int bytes, const Format format, const int channelNum);
    std::mutex m_mutex;

private:
    std::vector<AudioSink*> m_sinks;
};

class HotWordDetectorSink : public AudioSink{
public:
    struct HotWordDetectedParameterValue{
        int i;
        float f;
        std::shared_ptr<std::string> s;
    };
    using Observer = typename std::function<void(const std::unordered_map<std::string, HotWordDetectedParameterValue>&)>;
    void addObserver(const Observer observer);

protected:
    std::mutex m_mutex;
    std::vector<Observer> m_observers;
};
#ifdef R328PCMSOURCE
class R328PCMSource : public AudioSource{
public:
    static R328PCMSource* create(const int channelNum);
    void start();
    void stop();

private:
    R328PCMSource(void* recorder, const int channelNum);
    static void onAudio(const void *data, size_t size, size_t count);
    void* m_recorder;
    int m_channelNum;
    std::atomic_bool m_running;
};
#endif

class ALSASource : public AudioSource {
public:
    static ALSASource* create(const std::string& alsaDeviceName, const int channelNum, const int periodTime, const bool is32bit = false);
    static bool cset(const std::string& p1, const std::string& p2);
    void start();
    void stop();

private:
    ALSASource(snd_pcm_t* alsa, const int channelNum, const int periodTime, const bool is32bit = false);
    snd_pcm_t* m_alsa;
    int m_channelNum;
    bool m_is32bit;
    std::atomic_bool m_running;
    std::shared_ptr<std::thread> m_runner;
    int m_periodTime;
};

#ifdef CAE
class IFlytekCAE : public AudioSource, public HotWordDetectorSink {
public:
    static IFlytekCAE* create(const std::string& resPath, const int channelNum);
    void start();
    void stop();
    ~IFlytekCAE();
    void reload(const std::string& resPath);
    void auth(const std::string& token);

    void onAudio(const char *data, const int bytes, const Format format, const int channelNum) override;

private:
    static void cb_ivw(short angle, short channel, float power, short CMScore, short beam, char* p1, void* p2, void *userData);
    static void cb_audio(const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData);
    IFlytekCAE(const int channelNum);
    CAE_HANDLE m_cae;
    std::atomic_bool m_running;
    const int m_channelNum;
    uint32_t* m_caeBuf;
    std::atomic_bool m_reloading;
};
#endif//#ifdef CAE

#ifdef SNOWBOY
class Snowboy : public HotWordDetectorSink {
public:
    static Snowboy* create(const std::string& modelPath, const std::string& resPath);
    void start();
    void stop();
    void onAudio(const char *data, const int bytes, const Format format, const int channelNum) override;

private:
    Snowboy(std::shared_ptr<snowboy::SnowboyDetect> snowboy);
    ThreadPool m_executor;
    std::atomic_bool m_stop;
    std::shared_ptr<snowboy::SnowboyDetect> m_snowboy;
};
#endif

class SonicDecoder : public AudioSink {
public:
    using Observer = typename std::function<void(const std::string& data)>;
    void addObserver(const Observer observer);

private:
    std::vector<Observer> m_observers;
};

class FileWriterSink : public AudioSink {
public:
    void start();
    void stop();
    static FileWriterSink* create(const std::string& path);
    ~FileWriterSink();

protected:
    void onAudio(const char *data, const int bytes, const Format format, const int channelNum) override;

private:
    FileWriterSink(const std::string& path);
    const std::string m_path;
    int m_file;
    std::atomic_bool m_running;
    std::mutex m_mutex;
};

typedef alexaClientSDK::avsCommon::utils::sds::InProcessSDS::Reader SDSReader;
typedef alexaClientSDK::avsCommon::utils::sds::InProcessSDS::Writer SDSWriter;
typedef alexaClientSDK::avsCommon::utils::sds::InProcessSDS SDS;

class SharedStreamSink : public AudioSink {
public:
    static SharedStreamSink* create(int bufferSize);
    ~SharedStreamSink();
    std::shared_ptr<SDSReader> createReader();
    void onAudio(const char* data, const int bytes, const Format format, const int channelNum) override;

private:
    std::shared_ptr<SDS> m_stream;
    std::shared_ptr<SDSWriter> m_writer;
};

}
}



#endif //MESSAGER_IOTJS_MODULE_IFLYOS_AUDIO_H
