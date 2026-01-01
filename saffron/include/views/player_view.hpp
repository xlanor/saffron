#ifndef SAFFRON_PLAYER_VIEW_HPP
#define SAFFRON_PLAYER_VIEW_HPP

#include <borealis.hpp>
#include "views/video_view.hpp"
#include "core/mpv_core.hpp"
#include "core/plex_server.hpp"
#include "models/plex_types.hpp"

class VideoProfile;

class PlayerView : public brls::Box {
public:
    PlayerView(PlexServer* server, const plex::MediaItem& item,
               int bitrate = 0, const std::string& resolution = "",
               int mediaIndex = 0);
    ~PlayerView() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    brls::View* getDefaultFocus() override;

private:
    void initMpv();
    void setupOsd();
    void updateOsd();
    void showOsd();
    void hideOsd();
    void toggleOsd();
    void toggleProfile();

    void onStateChanged(PlaybackState state);
    void onProgress(int64_t position, int64_t duration);
    void onError(const std::string& error);
    void onEndFile(bool reachedEof);

    std::string formatTime(int64_t ms);
    void startPlayback();
    void startPlaybackWithOffset(int64_t offsetMs);
    void seekRelative(int64_t deltaMs);
    void executeSeek();
    void reportTimeline();
    void reportStopped();
    void playNextEpisode();
    bool isEpisode() const;

    PlexServer* m_server;
    plex::MediaItem m_item;
    int m_bitrate;
    std::string m_resolution;
    int m_mediaIndex;
    int64_t m_startOffset = 0;
    bool m_isDirectPlay = false;
    std::string m_sessionId;
    bool m_pendingSeek = false;
    int64_t m_lastReportedPosition = 0;
    int64_t m_pendingSeekDelta = 0;
    bool m_seekDebounceActive = false;

    VideoView* m_videoView = nullptr;
    VideoProfile* m_profile = nullptr;
    brls::Box* m_osdContainer = nullptr;
    brls::Box* m_topBar;
    brls::Box* m_bottomBar;
    brls::Label* m_titleLabel;
    brls::Label* m_timeLabel;
    brls::Slider* m_progressSlider;
    brls::Label* m_stateLabel;

    bool m_osdVisible = true;
    brls::RepeatingTask* m_osdTimer = nullptr;
    brls::RepeatingTask* m_timelineTimer = nullptr;
    int m_osdTimeout = 0;

    static constexpr int OSD_TIMEOUT_SECONDS = 5;

    static PlayerView* s_currentInstance;
    static bool s_isActive;
};

#endif
