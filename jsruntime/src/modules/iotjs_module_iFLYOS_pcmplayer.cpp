/* Copyright 2015-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern "C" {
#include "iotjs_def.h"
#include "iotjs_module_buffer.h"
}

#include "iotjs_module_iFLYOS_portaudiowrapper.h"
#include "iotjs_debuglog.h"

#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std::placeholders;

typedef std::function<void()> FinishForwardCallback;

class PcmPlayer {
public:
    PcmPlayer(int channels, int sampleFormat, int sampleRate)
        : m_channels(channels), m_sampleFormat(sampleFormat), m_sampleRate(sampleRate), m_vol(1) {
        auto cbStream = std::bind(&PcmPlayer::onStreamCallback, this, _1, _2, _3);
        auto cbFinish = std::bind(&PcmPlayer::onFinishCallback, this);
        m_paWrapper = new PortAudioWrapper(0, m_channels, m_sampleFormat, m_sampleRate, cbStream, cbFinish);

        if (m_sampleFormat == paInt8) {
            m_unitBytes = 1;
        } else if (m_sampleFormat == paInt16) {
            m_unitBytes = 2;
        } else if (m_sampleFormat == paInt24) {
            m_unitBytes = 3;
        } else if (m_sampleFormat == paInt32) {
            m_unitBytes = 4;
        }
    }

    ~PcmPlayer() {
        if (m_paWrapper) {
            delete m_paWrapper;
            m_paWrapper = nullptr;
        }
    }

    bool play(const uint8_t *data, int len, FinishForwardCallback cbForward) {
        if (!m_paWrapper->isStopped()) {
            m_paWrapper->abortStream();
            while (!m_paWrapper->isStopped()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        m_data = data;
        m_dataLen = len;
        m_offset = 0;
        m_cbForward = cbForward;
        return m_paWrapper->startStream();
    }

    void stop() {
        m_paWrapper->stopStream();
    }

    void setVolume(double volume) {
        if (volume >= 0 && volume <= 1) {
            m_vol = volume;
        }
    }

    void volumeDown() {
        m_vol = std::max(m_vol - 0.1, (double)0);
    }

    void volumeUp() {
        m_vol = std::min(m_vol + 0.1, (double)1);
    }

    int onStreamCallback(const void *input, void *output, unsigned long numSample) {
        if (m_unitBytes == 0) {
            return paAbort;
        }

        auto copyLen = std::min(m_unitBytes * m_channels * numSample, m_dataLen - m_offset);
        if (m_unitBytes == 2) {
            auto output16 = (short *)output;
            auto input16 = (short *)(m_data + m_offset);
            for (unsigned long i = 0; i < (copyLen / m_unitBytes); ++i) {
                output16[i] = input16[i] * m_vol;
            }
        }

        m_offset += copyLen;
        return m_dataLen <= m_offset ? paComplete : paContinue;
    }

    void onFinishCallback() {
        if (m_cbForward) {
            m_cbForward();
        }
    }

private:
    int m_channels;
    int m_sampleFormat;
    int m_sampleRate;
    PortAudioWrapper *m_paWrapper;

    double m_vol;

    const uint8_t *m_data;
    unsigned long m_dataLen;
    unsigned long m_offset;
    int m_unitBytes;

    FinishForwardCallback m_cbForward;
};

//////////////////////////////////////////////////////////////////////////////////////////

struct iotjs_pcmplayer_t {
    std::shared_ptr<PcmPlayer> player;
};

struct iotjs_pcmplayer_uvwork_t {
    std::shared_ptr<PcmPlayer> player;
    jerry_value_t jcallback;
    jerry_value_t jbuffer;
};

void iotjs_pcmplayer_empty(uv_work_t* req) {
}

void iotjs_after_pcmplayer_finish(uv_work_t* req, int status) {
    iotjs_pcmplayer_uvwork_t *work = static_cast<iotjs_pcmplayer_uvwork_t *>(req->data);

    if (!jerry_value_is_null(work->jcallback)) {
        iotjs_invoke_callback(work->jcallback, jerry_create_undefined(), nullptr, 0);
        jerry_release_value(work->jcallback);
    }

    jerry_release_value(work->jbuffer);
    delete work;
    delete req;
}

IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(pcmplayer);

static void iotjs_pcmplayer_destroy(iotjs_pcmplayer_t* pcmplayer) {
    if (pcmplayer->player) {
        pcmplayer->player.reset();
    }

    IOTJS_RELEASE(pcmplayer);
}

static void iotjs_pcmplayer_finish(iotjs_pcmplayer_uvwork_t *work) {
    auto req = new uv_work_t;
    req->data = static_cast<void *>(work);
    auto uv = iotjs_environment_loop(iotjs_environment_get());
    uv_queue_work(uv, req, iotjs_pcmplayer_empty, iotjs_after_pcmplayer_finish);
}

JS_FUNCTION(Create) {
    JS_CHECK_ARGS(4, object, number, number, number)

    const jerry_value_t jobject = JS_GET_ARG(0, object);
    int numChannels = JS_GET_ARG(1, number);
    int sampleFormat = JS_GET_ARG(2, number);
    int sampleRate = JS_GET_ARG(3, number);

    iotjs_pcmplayer_t* pcmplayer = (iotjs_pcmplayer_t*)iotjs_buffer_allocate(sizeof(iotjs_pcmplayer_t));
    pcmplayer->player = std::shared_ptr<PcmPlayer>(new PcmPlayer(numChannels, sampleFormat, sampleRate));
    jerry_set_object_native_pointer(jobject, pcmplayer, &this_module_native_info);
    return jerry_create_undefined();
}

JS_FUNCTION(Play) {
    JS_CHECK_ARGS(2, object, object)
    JS_DECLARE_OBJECT_PTR(0, pcmplayer, pcmplayer);

    auto jbuffer = JS_GET_ARG(1, object);
    jerry_acquire_value(jbuffer);
    iotjs_bufferwrap_t *bufferwrap = iotjs_bufferwrap_from_jbuffer(jbuffer);

    auto jcallback = JS_GET_ARG_IF_EXIST(2, function);
    if (!jerry_value_is_null(jcallback)) {
        jerry_acquire_value(jcallback);
    }

    iotjs_pcmplayer_uvwork_t *work = new iotjs_pcmplayer_uvwork_t;
    work->jbuffer = jbuffer;
    work->jcallback = jcallback;
    work->player = pcmplayer->player;
    auto cbForward = std::bind(iotjs_pcmplayer_finish, work);

    return jerry_create_boolean(pcmplayer->player->play((uint8_t *)bufferwrap->buffer, bufferwrap->length, cbForward));
}

JS_FUNCTION(Stop) {
    JS_CHECK_ARGS(1, object)

    JS_DECLARE_OBJECT_PTR(0, pcmplayer, pcmplayer);
    pcmplayer->player->stop();
    return jerry_create_null();
}

JS_FUNCTION(VolumeDown) {
    JS_CHECK_ARGS(1, object)

    JS_DECLARE_OBJECT_PTR(0, pcmplayer, pcmplayer);
    pcmplayer->player->volumeDown();
    return jerry_create_null();
}

JS_FUNCTION(VolumeUp) {
    JS_CHECK_ARGS(1, object)

    JS_DECLARE_OBJECT_PTR(0, pcmplayer, pcmplayer);
    pcmplayer->player->volumeUp();
    return jerry_create_null();
}

JS_FUNCTION(SetVolume) {
    JS_CHECK_ARGS(2, object, number)

    JS_DECLARE_OBJECT_PTR(0, pcmplayer, pcmplayer);
    pcmplayer->player->setVolume(JS_GET_ARG(1, number));
    return jerry_create_null();
}

extern "C" {
jerry_value_t InitPCMPlayer(void) {
    jerry_value_t player = jerry_create_object();

    iotjs_jval_set_method(player, "create", Create);
    iotjs_jval_set_method(player, "play", Play);
    iotjs_jval_set_method(player, "stop", Stop);
    iotjs_jval_set_method(player, "volumeDown", VolumeDown);
    iotjs_jval_set_method(player, "volumeUp", VolumeUp);
    iotjs_jval_set_method(player, "setVolume", SetVolume);
    return player;
}
}
