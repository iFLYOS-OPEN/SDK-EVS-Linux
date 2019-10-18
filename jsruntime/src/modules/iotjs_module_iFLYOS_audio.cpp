//
// Created by jan on 19-5-21.
//

#include <iostream>
#include <signal.h>
#include "iotjs_module_iFLYOS_audio.h"
#ifdef SNOWBOY
#include <snowboy-detect.h>
#endif

void iflyos::audio::AudioSource::connectSink(iflyos::audio::AudioSink* sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.emplace_back(sink);
}

void iflyos::audio::AudioSource::fireAudio(const char *data, const int bytes, const iflyos::audio::Format format,
                                           const int channelNum) {
    std::lock_guard<std::mutex> lock(m_mutex);
    //std::cout << "fire " << bytes << " from capture" << std::endl;
    for(auto& s : m_sinks){
        s->onAudio(data, bytes, format, channelNum);
    }
}

void iflyos::audio::HotWordDetectorSink::addObserver(const iflyos::audio::HotWordDetectorSink::Observer observer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.emplace_back(observer);
}

#ifdef SNOWBOY
void iflyos::audio::Snowboy::onAudio(const char *data, const int bytes, const iflyos::audio::Format format,
                                     const int channelNum) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_stop){
            return;
        }
    }
    //ubuntu pc should powerful to run detector on record thread
//    std::shared_ptr<int16_t> b = std::shared_ptr<int16_t>(new int16_t[bytes / sizeof(int16_t)]);
//    memcpy(b.get(), data, bytes);
//    m_executor.enqueue([this,b,bytes]{
//        if(m_stop) return;
    //std::cout << "snowboy to process " << bytes << " bytes" << std::endl;
//        auto result = m_snowboy->RunDetection(b.get(), bytes / sizeof(int16_t));
    auto result = m_snowboy->RunDetection((const int16_t *)data, bytes / sizeof(int16_t));
    if (result > 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for(auto& observer : m_observers) {
//            std::cout
//            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
//            << std::endl;
            observer(std::unordered_map<std::string, HotWordDetectedParameterValue>{});
        }
    }
//    });
}

iflyos::audio::Snowboy*
iflyos::audio::Snowboy::create(const std::string &modelPath, const std::string &resPath) {
    //std::string sensitivity_str = "0.5";
    float audio_gain = 1;
    bool apply_frontend = false;
    // Initializes Snowboy detector.
    //auto detector = std::shared_ptr<snowboy::SnowboyDetect>(makeNew(resPath.c_str(), modelPath.c_str()));
    auto detector = std::shared_ptr<snowboy::SnowboyDetect>(new snowboy::SnowboyDetect(resPath, modelPath));
    detector->SetSensitivity("0.5");
    //detector->SetSensitivity("0.5");
    //snowboy_c00_abi_set_sensitivity(detector.get(), "0.5");
    detector->SetAudioGain(audio_gain);
    detector->ApplyFrontend(apply_frontend);
    return new Snowboy(detector);
}

void iflyos::audio::Snowboy::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stop = false;
}

void iflyos::audio::Snowboy::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stop = true;
}

iflyos::audio::Snowboy::Snowboy(std::shared_ptr<snowboy::SnowboyDetect> snowboy) :
    m_executor{1}, m_stop{false}, m_snowboy{snowboy}{

}
#endif //SNOWBOY

void iflyos::audio::FileWriterSink::onAudio(const char *data, const int bytes, const iflyos::audio::Format format,
                                            const int channelNum) {
    if(!m_running)
        return;

    if(m_file < 0){
        m_file = open(m_path.c_str(), O_WRONLY | O_NONBLOCK);
    }

    if(m_file < 0){
        //FIFO reader not ready
        return;
    }
    //std::cout << "filewriter to write " << bytes << " bytes" << std::endl;
    auto wr = write(m_file, data, bytes);
    if(wr < 0){
        close(m_file);
        m_file = -1;
    }
}

iflyos::audio::FileWriterSink* iflyos::audio::FileWriterSink::create(const std::string &path) {
    return new FileWriterSink(path);
}

iflyos::audio::FileWriterSink::FileWriterSink(const std::string &path) : m_path{path}, m_running{false}, m_file{-1} {
}

void iflyos::audio::FileWriterSink::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = true;
}

void iflyos::audio::FileWriterSink::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
}

iflyos::audio::FileWriterSink::~FileWriterSink() {
    if(m_file > 0){
        close(m_file);
        m_file = -1;
    }
}

iflyos::audio::ALSASource*
iflyos::audio::ALSASource::create(const std::string &alsaDeviceName, const int channelNum, const int periodTime, const bool is32bit) {
    snd_pcm_t* alsa;
    snd_pcm_hw_params_t* param;

    std::cout << "to run ALSASource::create()" << std::endl;
    auto err = snd_pcm_open(&alsa, alsaDeviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err) {
        std::cerr << "open alsa device " << alsaDeviceName << " failed " << err << std::endl;
        return nullptr;
    }
    auto pErr = &err;
    ScopeGuard closeOnFail([pErr, alsa]{
        std::cout << "ALSASource::create()?" << ((*pErr < 0) ? "error" : "ok") << std::endl;
        if(*pErr < 0) {
            snd_pcm_close(alsa);
        }
    });

    snd_pcm_hw_params_alloca(&param);
    //ScopeGuard freeParam([param]{snd_pcm_hw_params_free(param);});

    err = snd_pcm_hw_params_any(alsa, param);
    if (err < 0){
        std::cerr << "snd_pcm_hw_params_any failed" << std::endl;
        return nullptr;
    }
    snd_pcm_hw_params_set_access(alsa, param, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(is32bit){
        err = snd_pcm_hw_params_set_format(alsa, param, SND_PCM_FORMAT_S32_LE);
    }else {
        err = snd_pcm_hw_params_set_format(alsa, param, SND_PCM_FORMAT_S16_LE);
    }
    if (err < 0) {
        std::cerr << "snd_pcm_hw_params_set_format failed" << std::endl;
        return nullptr;
    }
    err = snd_pcm_hw_params_set_channels(alsa, param, channelNum);
    if (err < 0) {
        std::cerr << "snd_pcm_hw_params_set_channels failed " << channelNum << std::endl;
        return nullptr;
    }
    err = snd_pcm_hw_params_set_rate(alsa, param, 16000, 0);
    if (err < 0) {
        std::cerr << "snd_pcm_hw_params_set_rate failed " << std::endl;
        return nullptr;
    }
    uint32_t buffer_time = 0;
    snd_pcm_hw_params_get_buffer_time_max(param, &buffer_time, 0);
    if (buffer_time > 200000)
        buffer_time = 200000;/*200000us = 200ms*/


    err = snd_pcm_hw_params_set_buffer_time_near(alsa, param, &buffer_time, 0);
    if (err < 0) {
        std::cerr << "snd_pcm_hw_params_set_buffer_time_near failed " << std::endl;
        return nullptr;
    }
    err = snd_pcm_hw_params_set_period_time(alsa, param, periodTime, 0);
    if (err < 0) {
        auto minV = 0u, maxV = 0u;
        snd_pcm_hw_params_get_period_time_min(param, &minV, nullptr);
        snd_pcm_hw_params_get_period_time_max(param, &maxV, nullptr);
        std::cerr << "snd_pcm_hw_params_set_period_time failed," << periodTime << "[" << minV << ", " << maxV << "]" << std::endl;
        return nullptr;
    }
    err = snd_pcm_hw_params(alsa, param);
    if (err < 0) {
        std::cerr << "snd_pcm_hw_params failed " << std::endl;
        return nullptr;
    }

    return new ALSASource(alsa, channelNum, periodTime, is32bit);
}

void iflyos::audio::ALSASource::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_running) return;
    m_running = true;
    m_runner = std::make_shared<std::thread>([this]{
        int sampleBytes = (m_is32bit ? 2 : 1);
        short buf[m_channelNum * 16 * m_periodTime * sampleBytes];
        int onceLog = 0;
        while(m_running){
            size_t samples = 0;
            while(samples < m_periodTime * 16){
                if(!(onceLog++)) {
                    std::cout << "to snd_pcm_readi() " << 16 * m_periodTime - samples << " samples" << std::endl;
                }
                auto r = snd_pcm_readi(m_alsa, &buf[samples * m_channelNum * sampleBytes], 16 * m_periodTime - samples);
                if (r == -EAGAIN || (r >= 0 && (size_t) r < 16 * m_periodTime - samples)) {
                    snd_pcm_wait(m_alsa, m_periodTime);
                } else if (r == -EPIPE){
                    snd_pcm_recover(m_alsa, r, 0);
                } else if (r == -ESTRPIPE) {

                } else if (r < 0) {
                    std::cout << "snd_pcm_readi() breaks " << r << std::endl;
                    return;
                }
                if (r > 0) {
                    samples += r;
                }
            }
            fireAudio((const char*)buf, sizeof(buf), m_is32bit ? Format::S32LE : Format::S16LE, m_channelNum);
        }
    });
}

void iflyos::audio::ALSASource::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_running) return;
    m_runner->join();
    m_running = false;
    m_runner.reset();
}

iflyos::audio::ALSASource::ALSASource(snd_pcm_t* alsa, const int channelNum, const int periodTime, const bool is32bit) :
    m_alsa{alsa}, m_channelNum{channelNum}, m_is32bit{is32bit},m_running{false}, m_runner{nullptr}, m_periodTime{periodTime / 1000} {

}

bool iflyos::audio::ALSASource::cset(const std::string &p1, const std::string &p2) {
    auto argv1 = p1.c_str();
    auto argv2 = p2.c_str();
    int err;
    static snd_ctl_t *handle = NULL;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);
    char card[64] = "default";

    if (snd_ctl_ascii_elem_id_parse(id, argv1)) {
        fprintf(stderr, "Wrong control identifier: %s\n", argv1);
        return false;
    }
    if (handle == NULL && (err = snd_ctl_open(&handle, card, 0)) < 0) {
        printf("Control %s open error: %s\n", card, snd_strerror(err));
        return false;
    }
    snd_ctl_elem_info_set_id(info, id);
    if ((err = snd_ctl_elem_info(handle, info)) < 0) {
        printf("Cannot find the given element from control %s\n", card);
        snd_ctl_close(handle);
        handle = NULL;
        return false;
    }
    snd_ctl_elem_info_get_id(info, id); /* FIXME: Remove it when hctl find works ok !!! */
    snd_ctl_elem_value_set_id(control, id);
    if ((err = snd_ctl_elem_read(handle, control)) < 0) {
        printf("Cannot read the given element from control %s\n", card);
        snd_ctl_close(handle);
        handle = NULL;
        return false;
    }
    err = snd_ctl_ascii_value_parse(handle, control, info, argv2);
    if (err < 0) {
        printf("Control %s parse error: %s\n", card, snd_strerror(err));
        snd_ctl_close(handle);
        handle = NULL;
        return false;
    }
    if ((err = snd_ctl_elem_write(handle, control)) < 0) {
        printf("Control %s element write error: %s\n", card,
               snd_strerror(err));
        snd_ctl_close(handle);
        handle = NULL;
        return false;
    }
    snd_ctl_close(handle);
    handle = NULL;
    return true;
}

#ifdef CAE
static void cb_ivw_audio(const void *audioData, unsigned int audioLen, int param1, const void *param2,
         void *userData) {}

iflyos::audio::IFlytekCAE *iflyos::audio::IFlytekCAE::create(const std::string &resPath, const int channelNum) {
    CAE_HANDLE  cae;
    auto ins = new IFlytekCAE(channelNum);
    auto ret = CAENew(&cae, resPath.c_str(), cb_ivw, cb_ivw_audio, cb_audio, nullptr, reinterpret_cast<void*>(ins));
    if(ret){
        std::cerr << "CAENew Failed " << ret <<std::endl;
        return nullptr;
    }
    //CAEResetEng(cae);
    ins->m_cae = cae;
    return ins;
}

void iflyos::audio::IFlytekCAE::reload(const std::string& resPath) {
    m_reloading = true;
    auto ret = CAEReloadResource(m_cae, resPath.c_str());
    if(ret){
        std::cerr << "CAEReloadResource Failed " << ret <<std::endl;
    }
    m_reloading = false;
}

void iflyos::audio::IFlytekCAE::start() {
    m_running = true;
}

void iflyos::audio::IFlytekCAE::stop() {
    m_running = false;
}

#define MAX_SAMPLES_PER_FRAME (64 * 16)//64ms 16k max
iflyos::audio::IFlytekCAE::IFlytekCAE(const int channelNum)
: m_running{false}, m_channelNum{channelNum}, m_reloading{false} {
    static uint32_t caePreSet[]=  {0x00000100, 0x00000200, 0x00000300, 0x00000400, 0x00000500, 0x00000600, 0x00000700, 0x00000800};
    m_caeBuf = new uint32_t[m_channelNum * MAX_SAMPLES_PER_FRAME];
    for(unsigned int i = 0; i < MAX_SAMPLES_PER_FRAME; i++) {
        for(size_t j = 0; j < m_channelNum; j++){
            m_caeBuf[i * m_channelNum + j] = caePreSet[j];
        }
    }
}

void iflyos::audio::IFlytekCAE::onAudio(const char *data, const int bytes, const iflyos::audio::Format format,
                                        const int channelNum) {
    //std::cout << "to write cae check:" << bytes <<std::endl;
    assert(m_channelNum == channelNum);

    //cae will start new processing thread, so should not block here
    if(!m_reloading && m_running){
        void* caeData = m_caeBuf;
        int caeBytes = bytes * 2;
        if(format == Format::S16LE) {
            assert(bytes <= MAX_SAMPLES_PER_FRAME * m_channelNum * sizeof(short));
            assert(bytes % (sizeof(short) * channelNum) == 0);
            auto inBuf = (short *) data;
            for (unsigned int i = 0; i < bytes / sizeof(short) / channelNum; i++) {
                int baseIdx = i * m_channelNum;
                for (int n = 0; n < m_channelNum; ++n) {
                    char *out2 = (char *) (&m_caeBuf[baseIdx + n]);
                    char *s16 = (char *) (&inBuf[baseIdx + n]);
                    out2[2] = s16[0];
                    out2[3] = s16[1];
                }
            }
        }else{
            caeData = (void*)data;
            caeBytes = bytes;
        }
        //std::cout << "to write cae:" << bytes  * 2 <<std::endl;
        auto ret = CAEAudioWrite(m_cae, (const void*)caeData, caeBytes);
        if(ret){
            std::cerr << "CAE Write Failed " << ret <<std::endl;
        }
    }
}

void
iflyos::audio::IFlytekCAE::cb_ivw(short angle, short channel, float power, short CMScore, short beam
    , char* p1, void* p2, void *userData) {
    auto thisObj = (IFlytekCAE*)userData;
    //CAEResetEng(thisObj->m_cae);
    //CAESetRealBeam(thisObj->m_cae, beam);
    {
        HotWordDetectorSink::HotWordDetectedParameterValue v1 = {angle, 0.0, nullptr};
        HotWordDetectorSink::HotWordDetectedParameterValue v2 = {channel, 0.0, nullptr};
        HotWordDetectorSink::HotWordDetectedParameterValue v3 = {0, power, nullptr};
        HotWordDetectorSink::HotWordDetectedParameterValue v4 = {CMScore, 0.0, nullptr};
        HotWordDetectorSink::HotWordDetectedParameterValue v5 = {beam, 0.0, nullptr};
        std::unordered_map<std::string, HotWordDetectedParameterValue> params;
        params.emplace("angle", v1);
        params.emplace("channel", v2);
        params.emplace("power", v3);
        params.emplace("CMScore", v4);
        params.emplace("beam", v5);
        {
            std::lock_guard<std::mutex> lock(thisObj->iflyos::audio::HotWordDetectorSink::m_mutex);
            for (auto observer : thisObj->m_observers) {
                observer(params);
            }
        }

    }
}

void iflyos::audio::IFlytekCAE::cb_audio(const void *audioData, unsigned int audioLen, int param1, const void *param2,
                                         void *userData) {
    auto thisObj = (IFlytekCAE*)userData;
    thisObj->fireAudio((const char*)audioData, audioLen, Format::S16LE, 1);
}

iflyos::audio::IFlytekCAE::~IFlytekCAE() {
    m_running = false;
    usleep(MAX_SAMPLES_PER_FRAME  / 16 * 1000);//64ms, make no CAE writing
    CAEDestroy(m_cae);
}

void iflyos::audio::IFlytekCAE::auth(const std::string &token) {
    auto ret = CAEAuth((char*)token.c_str());
    if(ret){
        printf("CAEAuth() failed with %d\n", ret);
    }
}

#endif //#ifdef CAE

iflyos::audio::SharedStreamSink *iflyos::audio::SharedStreamSink::create(int bufferSize) {
    auto buffer = std::make_shared<SDS::Buffer>(bufferSize);
    std::shared_ptr<SDS> sharedDataStream = SDS::create(buffer, 2, 1);

    if (!sharedDataStream) {
        std::cerr << "Failed to create shared data stream!" << std::endl;
        return nullptr;
    }

    //SDSWriter::Policy::NONBLOCKABLE makes writing not wait reading
    auto writer = sharedDataStream->createWriter(SDSWriter::Policy::NONBLOCKABLE);
    auto ret = new SharedStreamSink();
    ret->m_stream = sharedDataStream;
    ret->m_writer = std::move(writer);
    return ret;
}

iflyos::audio::SharedStreamSink::~SharedStreamSink() {
    m_writer.reset();
    m_stream.reset();
}

std::shared_ptr<iflyos::audio::SDSReader> iflyos::audio::SharedStreamSink::createReader() {
    auto ret = m_stream->createReader(SDSReader::Policy::NONBLOCKING, true);
    if(!ret){
        std::cerr << "SharedStreamSink create reader failed" << std::endl;
    }

    //roll back 60ms?
    if(!ret->seek(3 * 320, SDSReader::Reference::BEFORE_WRITER)){
        std::cerr << "roll back (3 * 640)bytes failed" << std::endl;
    }
    return ret;
}

void iflyos::audio::SharedStreamSink::onAudio(const char *data, const int bytes, const iflyos::audio::Format format,
                                              const int channelNum) {
    auto ret = m_writer->write(data, bytes / 2);
    if(ret < 0) {
        std::cerr << "SharedStreamSink write failed:" << ret << std::endl;
    }
}

#ifdef R328PCMSOURCE

/** 录音数据回调 **/
typedef void (*record_data_cb_fn)(const void *data, size_t size, size_t count);

/** 录音开始 **/
int record_start(void**record_hd, record_data_cb_fn recordCb);

/** 录音结束 **/
void record_stop(void* record_hd);

static iflyos::audio::R328PCMSource* ins = nullptr;

iflyos::audio::R328PCMSource *iflyos::audio::R328PCMSource::create(const int channelNum) {
    if(ins){
        printf("ERROR!!!only one recorder can be reated\n");
        return nullptr;
    }

    void* recorder;
    auto ret = record_start(&recorder, iflyos::audio::R328PCMSource::onAudio);
    if (0 != ret){
        printf("record_start failed ret = %d \n", ret);
        return nullptr;
    }
    return new iflyos::audio::R328PCMSource(recorder, channelNum);
}

void iflyos::audio::R328PCMSource::start() {
    m_running = true;
}

void iflyos::audio::R328PCMSource::stop() {
    m_running = false;
}

iflyos::audio::R328PCMSource::R328PCMSource(void* recorder, const int channelNum)
        : m_recorder{recorder}, m_channelNum{channelNum}, m_running{false}{
    ins = this;
}

void iflyos::audio::R328PCMSource::onAudio(const void *data, size_t size, size_t count) {
    //printf("iflyos::audio::R328PCMSource::onAudio(%p, %d, %d)\n", data, size, count);
    if(!ins) return;
    if(ins->m_running){
        ins->fireAudio((const char*)data, (const int)(size*count), Format::S32LE, ins->m_channelNum);
    }
}

#endif//#ifdef R328PCMSOURCE
