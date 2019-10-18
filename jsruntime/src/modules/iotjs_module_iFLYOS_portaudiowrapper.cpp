#include "iotjs_module_iFLYOS_portaudiowrapper.h"
#include <iostream>
#include <thread>

static int paStreamCallback_s(const void *input, void *output, unsigned long numSamples,
                        const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                        void *userData) {
    if (!userData) {
        return paAbort;
    }

    return ((PortAudioWrapper *) userData)->onStreamCallback(input, output, numSamples);
}

static void paFinishCallback_s(void *userData) {
    if (userData) {
        ((PortAudioWrapper *) userData)->onFinishCallback();
    }
}

PortAudioWrapper::PortAudioWrapper(int numInput,
                                   int numOutput,
                                   PaSampleFormat sampleFormat,
                                   double sampleRate,
                                   StreamCallback cbStream,
                                   FinishCallback cbFinish)
    : m_paStream(nullptr), m_cbStream(std::move(cbStream)), m_cbFinish(std::move(cbFinish)) {
    PaError err;
    if ((err = Pa_Initialize()) != paNoError) {
        std::cout << "Failed to initialize PortAudio" << std::endl;
    } else if ((err = Pa_OpenDefaultStream(&m_paStream, numInput, numOutput, sampleFormat, sampleRate,
                                           paFramesPerBufferUnspecified, paStreamCallback_s, this)) != paNoError) {
        std::cout << "Failed to open PortAudio default stream" << std::endl;
    } else {
        Pa_SetStreamFinishedCallback(m_paStream, paFinishCallback_s);
    }
}

PortAudioWrapper::~PortAudioWrapper() {
    if (m_paStream) {
        Pa_StopStream(m_paStream);
        while (!isStopped()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Pa_CloseStream(m_paStream);
    }
    Pa_Terminate();
}

bool PortAudioWrapper::startStream() {
    if (!m_paStream) {
        std::cout << "PortAudio stream has not been opened" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    PaError err = Pa_StartStream(m_paStream);
    if (err != paNoError) {
        std::cout << "Failed to start PortAudio stream" << std::endl;
    }
    return err == paNoError;
}

bool PortAudioWrapper::stopStream() {
    if (!m_paStream) {
        std::cout << "PortAudio stream has not been opened" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (Pa_IsStreamStopped(m_paStream) == 1) {
        return true;
    }

    PaError err = Pa_StopStream(m_paStream);
    if (err != paNoError) {
        std::cout << "Failed to stop PortAudio stream" << std::endl;
    }
    return err == paNoError;
}

bool PortAudioWrapper::abortStream() {
    if (!m_paStream) {
        std::cout << "PortAudio stream has not been opened" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (Pa_IsStreamStopped(m_paStream) == 1) {
        return true;
    }

    PaError err = Pa_AbortStream(m_paStream);
    if (err != paNoError) {
        std::cout << "Failed to abort PortAudio stream" << std::endl;
    }
    return err == paNoError;
}

bool PortAudioWrapper::isActive() {
    return m_paStream && Pa_IsStreamActive(m_paStream) == 1;
}

bool PortAudioWrapper::isStopped() {
    return !m_paStream || Pa_IsStreamStopped(m_paStream) == 1;
}

int PortAudioWrapper::onStreamCallback(const void *input, void *output, unsigned long numSamples) {
    return m_cbStream((const char *) input, (char *) output, numSamples);
}

void PortAudioWrapper::onFinishCallback() {
    if (m_cbFinish) {
        m_cbFinish();
    }
}
