#ifndef IOTJS_MODULE_IFLYOS_PORTAUDIOWRAPPER_H
#define IOTJS_MODULE_IFLYOS_PORTAUDIOWRAPPER_H

#include <portaudio.h>
#include <mutex>
#include <functional>

typedef std::function<int(const char *, char *, unsigned long)> StreamCallback;
typedef std::function<void()> FinishCallback;

class PortAudioWrapper {
public:
    PortAudioWrapper(int numInput,
                     int numOutput,
                     PaSampleFormat sampleFormat,
                     double sampleRate,
                     StreamCallback cbStream,
                     FinishCallback cbFinish = FinishCallback());
    PortAudioWrapper(PortAudioWrapper &) = delete;
    virtual ~PortAudioWrapper();

    bool startStream();
    bool stopStream();
    bool abortStream();

    bool isActive();
    bool isStopped();

    int onStreamCallback(const void *input, void *output, unsigned long numSamples);
    void onFinishCallback();

private:
    PaStream *m_paStream;
    std::mutex m_mutex;
    StreamCallback m_cbStream;
    FinishCallback m_cbFinish;
};

#endif //IOTJS_MODULE_IFLYOS_PORTAUDIOWRAPPER_H