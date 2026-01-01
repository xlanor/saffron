#include "core/mpv_core.hpp"
#include "core/settings_manager.hpp"

#include <cstring>

#ifdef BOREALIS_USE_DEKO3D
#include <borealis/platforms/switch/switch_video.hpp>
#endif

MPVCore* MPVCore::s_instance = nullptr;

static inline void checkError(int status) {
    if (status < 0) {
        brls::Logger::error("MPV error: {}", mpv_error_string(status));
    }
}

MPVCore* MPVCore::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new MPVCore();
    }
    return s_instance;
}

void MPVCore::destroyInstance() {
    if (s_instance != nullptr) {
        delete s_instance;
        s_instance = nullptr;
    }
}

MPVCore::MPVCore() {
#ifdef BOREALIS_USE_DEKO3D
    memset(&m_doneFence, 0, sizeof(m_doneFence));
    memset(&m_readyFence, 0, sizeof(m_readyFence));
    m_mpvFbo = {
        .tex = nullptr,
        .ready_fence = &m_readyFence,
        .done_fence = &m_doneFence,
        .w = 1280,
        .h = 720,
        .format = DkImageFormat_RGBA8_Unorm,
    };
    m_mpvParams[0] = {MPV_RENDER_PARAM_DEKO3D_FBO, &m_mpvFbo};
    m_mpvParams[1] = {MPV_RENDER_PARAM_INVALID, nullptr};
#else
    m_mpvFbo = {0, 1280, 720, 0};
    m_mpvParams[0] = {MPV_RENDER_PARAM_OPENGL_FBO, &m_mpvFbo};
    m_mpvParams[1] = {MPV_RENDER_PARAM_FLIP_Y, &m_flipY};
    m_mpvParams[2] = {MPV_RENDER_PARAM_INVALID, nullptr};
#endif
}

MPVCore::~MPVCore() {
    cleanup();
}

void MPVCore::init() {
    if (m_mpv) return;

    enableSyncCallbacks();
    setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv) {
        brls::Logger::error("Failed to create MPV handle");
        return;
    }

    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "hwdec", "auto");
    mpv_set_option_string(m_mpv, "osd-level", "1");
    mpv_set_option_string(m_mpv, "video-timing-offset", "0");
    mpv_set_option_string(m_mpv, "video-sync",
        SettingsManager::getInstance()->getVideoSyncMode().c_str());
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "ytdl", "no");

    int cacheSize = SettingsManager::getInstance()->getInMemoryCache();
    if (cacheSize > 0) {
        std::string cacheStr = std::to_string(cacheSize) + "MiB";
        std::string backCacheStr = std::to_string(cacheSize / 2) + "MiB";
        brls::Logger::info("MPV cache: {} forward, {} back", cacheStr, backCacheStr);
        mpv_set_option_string(m_mpv, "demuxer-max-bytes", cacheStr.c_str());
        mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", backCacheStr.c_str());
        mpv_set_option_string(m_mpv, "demuxer-readahead-secs", "600");
        mpv_set_option_string(m_mpv, "cache", "yes");
    } else {
        mpv_set_option_string(m_mpv, "cache", "no");
    }

    mpv_set_option_string(m_mpv, "network-timeout", "30");

    mpv_set_option_string(m_mpv, "terminal", "yes");
    mpv_set_option_string(m_mpv, "msg-level", "all=v");

#ifdef __SWITCH__
    mpv_set_option_string(m_mpv, "vd-lavc-dr", "yes");
    mpv_set_option_string(m_mpv, "vd-lavc-threads", "3");
    mpv_set_option_string(m_mpv, "opengl-glfinish", "yes");
    mpv_set_option_string(m_mpv, "audio-channels", "stereo");
#endif

    if (mpv_initialize(m_mpv) < 0) {
        brls::Logger::error("Failed to initialize MPV");
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    checkError(mpv_observe_property(m_mpv, 1, "core-idle", MPV_FORMAT_FLAG));
    checkError(mpv_observe_property(m_mpv, 2, "pause", MPV_FORMAT_FLAG));
    checkError(mpv_observe_property(m_mpv, 3, "duration", MPV_FORMAT_INT64));
    checkError(mpv_observe_property(m_mpv, 4, "playback-time", MPV_FORMAT_DOUBLE));
    checkError(mpv_observe_property(m_mpv, 5, "volume", MPV_FORMAT_INT64));
    checkError(mpv_observe_property(m_mpv, 6, "speed", MPV_FORMAT_DOUBLE));

#ifdef BOREALIS_USE_DEKO3D
    auto* videoContext = dynamic_cast<brls::SwitchVideoContext*>(
        brls::Application::getPlatform()->getVideoContext()
    );
    mpv_deko3d_init_params dekoInitParams{videoContext->getDeko3dDevice()};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_DEKO3D)},
        {MPV_RENDER_PARAM_DEKO3D_INIT_PARAMS, &dekoInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#else
    mpv_opengl_init_params glInitParams{nullptr, nullptr};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#endif

    if (mpv_render_context_create(&m_mpvContext, m_mpv, params) < 0) {
        brls::Logger::error("Failed to create MPV render context");
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    mpv_set_wakeup_callback(m_mpv, onWakeup, this);
    mpv_render_context_set_update_callback(m_mpvContext, onUpdate, this);

    m_focusSubscription = brls::Application::getWindowFocusChangedEvent()->subscribe(
        [this](bool focus) {
            if (!focus && m_state == PlaybackState::Playing) {
                pause();
            }
        }
    );

    m_exitSubscription = brls::Application::getExitEvent()->subscribe([this]() {
        cleanup();
    });

    brls::Logger::info("MPV initialized: {}",
        mpv_get_property_string(m_mpv, "mpv-version"));
}

void MPVCore::cleanup() {
    m_videoStopped.store(true);
    disableSyncCallbacks();
    clearCallbacks();

    if (m_mpv) {
        mpv_command_string(m_mpv, "quit");
    }

    brls::Application::getWindowFocusChangedEvent()->unsubscribe(m_focusSubscription);

    if (m_mpvContext) {
        mpv_render_context_free(m_mpvContext);
        m_mpvContext = nullptr;
    }

    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }

    m_state = PlaybackState::Stopped;
    m_position = 0;
    m_duration = 0;
}

void MPVCore::onWakeup(void* ctx) {
    MPVCore* core = static_cast<MPVCore*>(ctx);
    if (!core || !core->m_mpv) return;
    brls::sync([core]() {
        if (core->m_mpv) {
            core->eventLoop();
        }
    });
}

void MPVCore::onUpdate(void* ctx) {
    MPVCore* core = static_cast<MPVCore*>(ctx);
    if (!core || !core->m_mpvContext) return;
    brls::sync([core]() {
        if (core->m_mpvContext) {
            mpv_render_context_update(core->m_mpvContext);
        }
    });
}

void MPVCore::enableSyncCallbacks() {
    std::lock_guard<std::mutex> lock(m_syncMutex);
    m_alive = std::make_shared<bool>(true);
}

void MPVCore::disableSyncCallbacks() {
    std::lock_guard<std::mutex> lock(m_syncMutex);
    m_alive.reset();
}

void MPVCore::eventLoop() {
    while (m_mpv) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
            case MPV_EVENT_PROPERTY_CHANGE:
                handlePropertyChange(static_cast<mpv_event_property*>(event->data));
                break;

            case MPV_EVENT_START_FILE:
                m_state = PlaybackState::Buffering;
                if (m_onStateChanged) m_onStateChanged(m_state);
                break;

            case MPV_EVENT_FILE_LOADED:
                m_videoStopped.store(false);
                m_state = PlaybackState::Playing;
                if (m_onStateChanged) m_onStateChanged(m_state);
                break;

            case MPV_EVENT_END_FILE: {
                m_videoStopped.store(true);
                auto* endFile = static_cast<mpv_event_end_file*>(event->data);
                bool reachedEof = (endFile->reason == MPV_END_FILE_REASON_EOF);

                if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
                    std::string error = mpv_error_string(endFile->error);
                    if (m_onError) m_onError(error);
                }

                m_state = PlaybackState::Stopped;
                if (m_onStateChanged) m_onStateChanged(m_state);
                if (m_onEndFile) m_onEndFile(reachedEof);
                break;
            }

            case MPV_EVENT_SHUTDOWN:
                m_videoStopped.store(true);
                m_state = PlaybackState::Stopped;
                if (m_onStateChanged) m_onStateChanged(m_state);
                break;

            default:
                break;
        }
    }
}

void MPVCore::handlePropertyChange(mpv_event_property* prop) {
    if (strcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG) {
        bool paused = *static_cast<int*>(prop->data);
        if (m_state != PlaybackState::Stopped) {
            m_state = paused ? PlaybackState::Paused : PlaybackState::Playing;
            if (m_onStateChanged) m_onStateChanged(m_state);
        }
    } else if (strcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_INT64) {
        m_duration = *static_cast<int64_t*>(prop->data) * 1000;
    } else if (strcmp(prop->name, "playback-time") == 0 && prop->format == MPV_FORMAT_DOUBLE) {
        m_position = static_cast<int64_t>(*static_cast<double*>(prop->data) * 1000);
        if (m_onProgress) m_onProgress(m_position, m_duration);
    } else if (strcmp(prop->name, "volume") == 0 && prop->format == MPV_FORMAT_INT64) {
        m_volume = static_cast<int>(*static_cast<int64_t*>(prop->data));
    } else if (strcmp(prop->name, "speed") == 0 && prop->format == MPV_FORMAT_DOUBLE) {
        m_speed = *static_cast<double*>(prop->data);
    }
}

void MPVCore::play(const std::string& url, bool isDirectPlay, int bitrateKbps) {
    if (!m_mpv) {
        init();
    }

    if (!m_mpv) {
        if (m_onError) m_onError("MPV not initialized");
        return;
    }

    if (isDirectPlay) {
        mpv_set_property_string(m_mpv, "stream-lavf-o", "reconnect=1,reconnect_streamed=1");
        mpv_set_property_string(m_mpv, "cache-pause-wait", "2");
        brls::Logger::info("Direct play: {}kbps", bitrateKbps);
    } else {
        mpv_set_property_string(m_mpv, "stream-lavf-o",
            "reconnect=1,reconnect_streamed=1,reconnect_delay_max=2,seekable=0");
    }

    m_videoStopped.store(false);
    brls::Logger::info("Playing ({}): {}", isDirectPlay ? "direct" : "transcode", url);

    const char* cmd[] = {"loadfile", url.c_str(), nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MPVCore::stop() {
    if (!m_mpv) return;

    m_videoStopped.store(true);

    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, cmd);

    m_state = PlaybackState::Stopped;
    m_position = 0;
    if (m_onStateChanged) m_onStateChanged(m_state);
}

void MPVCore::pause() {
    if (!m_mpv) return;

    mpv_set_property_string(m_mpv, "pause", "yes");
}

void MPVCore::resume() {
    if (!m_mpv) return;

    mpv_set_property_string(m_mpv, "pause", "no");
}

void MPVCore::togglePlay() {
    if (m_state == PlaybackState::Playing) {
        pause();
    } else if (m_state == PlaybackState::Paused) {
        resume();
    }
}

void MPVCore::seek(int64_t positionMs) {
    if (!m_mpv) return;

    double positionSec = positionMs / 1000.0;
    std::string posStr = std::to_string(positionSec);
    const char* cmd[] = {"seek", posStr.c_str(), "absolute", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MPVCore::seekRelative(int64_t deltaMs) {
    if (!m_mpv) return;

    double deltaSec = deltaMs / 1000.0;
    std::string deltaStr = std::to_string(deltaSec);
    const char* cmd[] = {"seek", deltaStr.c_str(), "relative", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MPVCore::setVolume(int volume) {
    if (!m_mpv) return;

    m_volume = std::max(0, std::min(100, volume));
    int64_t vol = m_volume;
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_INT64, &vol);
}

int MPVCore::getVolume() const {
    return m_volume;
}

void MPVCore::setSpeed(double speed) {
    if (!m_mpv) return;

    m_speed = std::max(0.25, std::min(4.0, speed));
    mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &m_speed);
}

double MPVCore::getSpeed() const {
    return m_speed;
}

void MPVCore::setSubtitleTrack(int trackId) {
    if (!m_mpv) return;

    int64_t id = trackId;
    mpv_set_property(m_mpv, "sid", MPV_FORMAT_INT64, &id);
}

int MPVCore::getSubtitleTrack() const {
    if (!m_mpv) return 0;

    int64_t id = 0;
    mpv_get_property(m_mpv, "sid", MPV_FORMAT_INT64, &id);
    return static_cast<int>(id);
}

void MPVCore::cycleSubtitles() {
    if (!m_mpv) return;

    const char* cmd[] = {"cycle", "sub", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MPVCore::setAudioTrack(int trackId) {
    if (!m_mpv) return;

    int64_t id = trackId;
    mpv_set_property(m_mpv, "aid", MPV_FORMAT_INT64, &id);
}

int MPVCore::getAudioTrack() const {
    if (!m_mpv) return 0;

    int64_t id = 0;
    mpv_get_property(m_mpv, "aid", MPV_FORMAT_INT64, &id);
    return static_cast<int>(id);
}

void MPVCore::cycleAudio() {
    if (!m_mpv) return;

    const char* cmd[] = {"cycle", "audio", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

std::string MPVCore::getString(const std::string& key) {
    if (!m_mpv) return "";
    char* value = nullptr;
    mpv_get_property(m_mpv, key.c_str(), MPV_FORMAT_STRING, &value);
    if (!value) return "";
    std::string result(value);
    mpv_free(value);
    return result;
}

int64_t MPVCore::getInt(const std::string& key, int64_t defaultValue) {
    if (!m_mpv) return defaultValue;
    int64_t value = defaultValue;
    mpv_get_property(m_mpv, key.c_str(), MPV_FORMAT_INT64, &value);
    return value;
}

double MPVCore::getDouble(const std::string& key) {
    if (!m_mpv) return 0.0;
    double value = 0.0;
    mpv_get_property(m_mpv, key.c_str(), MPV_FORMAT_DOUBLE, &value);
    return value;
}

void MPVCore::setOnStateChanged(StateCallback callback) {
    m_onStateChanged = callback;
}

void MPVCore::setOnProgress(ProgressCallback callback) {
    m_onProgress = callback;
}

void MPVCore::setOnError(ErrorCallback callback) {
    m_onError = callback;
}

void MPVCore::setOnEndFile(EndFileCallback callback) {
    m_onEndFile = callback;
}

void MPVCore::clearCallbacks() {
    m_onStateChanged = nullptr;
    m_onProgress = nullptr;
    m_onError = nullptr;
    m_onEndFile = nullptr;
}

void MPVCore::setFrameSize(brls::Rect rect) {
    m_rect = rect;

#ifdef BOREALIS_USE_DEKO3D
    m_mpvFbo.w = static_cast<int>(rect.getWidth());
    m_mpvFbo.h = static_cast<int>(rect.getHeight());
#else
    m_mpvFbo.w = static_cast<int>(rect.getWidth());
    m_mpvFbo.h = static_cast<int>(rect.getHeight());
#endif
}

void MPVCore::draw(brls::Rect rect, float alpha) {
    if (!m_mpvContext) return;
    if (m_videoStopped.load()) return;
    if (alpha < 1.0f) return;

    setFrameSize(rect);

#ifdef BOREALIS_USE_DEKO3D
    static auto* videoContext = dynamic_cast<brls::SwitchVideoContext*>(
        brls::Application::getPlatform()->getVideoContext()
    );
    if (!videoContext) return;

    m_mpvFbo.tex = videoContext->getFramebuffer();
    if (!m_mpvFbo.tex) return;

    videoContext->queueSignalFence(&m_readyFence);
    videoContext->queueFlush();
#endif

    mpv_render_context_render(m_mpvContext, m_mpvParams);

#ifdef BOREALIS_USE_DEKO3D
    videoContext->queueWaitFence(&m_doneFence);
    videoContext->queueFlush();
    dkFenceWait(&m_doneFence, -1);  // CPU wait for MPV render to complete
#endif

    mpv_render_context_report_swap(m_mpvContext);
}
