#ifndef SAFFRON_MPV_CORE_HPP
#define SAFFRON_MPV_CORE_HPP

#include <borealis.hpp>
#include <mpv/client.h>

#ifdef BOREALIS_USE_DEKO3D
#include <mpv/render_dk3d.h>
#else
#include <mpv/render_gl.h>
#endif

#include <functional>
#include <string>
#include <atomic>

enum class PlaybackState {
    Stopped,
    Playing,
    Paused,
    Buffering
};

class MPVCore {
public:
    using StateCallback = std::function<void(PlaybackState)>;
    using ProgressCallback = std::function<void(int64_t position, int64_t duration)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using EndFileCallback = std::function<void(bool reachedEof)>;

    static MPVCore* getInstance();
    static void destroyInstance();

    void init();
    void cleanup();

    void play(const std::string& url, bool isDirectPlay = false, int bitrateKbps = 0);
    void stop();
    void pause();
    void resume();
    void togglePlay();
    void seek(int64_t positionMs);
    void seekRelative(int64_t deltaMs);

    void setVolume(int volume);
    int getVolume() const;

    void setSpeed(double speed);
    double getSpeed() const;

    void setSubtitleTrack(int trackId);
    int getSubtitleTrack() const;
    void cycleSubtitles();

    void setAudioTrack(int trackId);
    int getAudioTrack() const;
    void cycleAudio();

    std::string getString(const std::string& key);
    int64_t getInt(const std::string& key, int64_t defaultValue = 0);
    double getDouble(const std::string& key);

    PlaybackState getState() const { return m_state; }
    int64_t getPosition() const { return m_position; }
    int64_t getDuration() const { return m_duration; }
    bool isPlaying() const { return m_state == PlaybackState::Playing; }
    bool isPaused() const { return m_state == PlaybackState::Paused; }
    bool isStopped() const { return m_state == PlaybackState::Stopped; }

    void setOnStateChanged(StateCallback callback);
    void setOnProgress(ProgressCallback callback);
    void setOnError(ErrorCallback callback);
    void setOnEndFile(EndFileCallback callback);
    void clearCallbacks();

    void disableSyncCallbacks();
    void enableSyncCallbacks();

    void draw(brls::Rect rect, float alpha = 1.0f);
    void setFrameSize(brls::Rect rect);

    bool isValid() const { return m_mpv != nullptr && m_mpvContext != nullptr; }

private:
    MPVCore();
    ~MPVCore();

    MPVCore(const MPVCore&) = delete;
    MPVCore& operator=(const MPVCore&) = delete;

    void eventLoop();
    void handlePropertyChange(mpv_event_property* prop);

    static void onWakeup(void* ctx);
    static void onUpdate(void* ctx);

    static MPVCore* s_instance;

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_mpvContext = nullptr;

    PlaybackState m_state = PlaybackState::Stopped;
    int64_t m_position = 0;
    int64_t m_duration = 0;
    int m_volume = 100;
    double m_speed = 1.0;
    std::atomic<bool> m_videoStopped{true};

    brls::Rect m_rect = {0, 0, 1280, 720};

    StateCallback m_onStateChanged;
    ProgressCallback m_onProgress;
    ErrorCallback m_onError;
    EndFileCallback m_onEndFile;

    brls::Event<bool>::Subscription m_focusSubscription;
    brls::VoidEvent::Subscription m_exitSubscription;

    std::shared_ptr<bool> m_alive;
    std::mutex m_syncMutex;

#ifdef BOREALIS_USE_DEKO3D
    DkFence m_doneFence;
    DkFence m_readyFence;
    mpv_deko3d_fbo m_mpvFbo;
    mpv_render_param m_mpvParams[3];
#else
    mpv_opengl_fbo m_mpvFbo;
    int m_flipY = 1;
    mpv_render_param m_mpvParams[3];
#endif
};

#endif
