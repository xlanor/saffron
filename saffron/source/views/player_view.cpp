#include "views/player_view.hpp"
#include "views/video_profile.hpp"
#include "core/plex_api.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "util/image_loader.hpp"
#include "util/overclock.hpp"

PlayerView* PlayerView::s_currentInstance = nullptr;
bool PlayerView::s_isActive = false;

class OsdTask : public brls::RepeatingTask {
public:
    OsdTask(brls::Time period, std::function<void()> callback)
        : brls::RepeatingTask(period), m_callback(std::move(callback)) {}
    void run() override { if (m_callback) m_callback(); }
private:
    std::function<void()> m_callback;
};

class TimelineTask : public brls::RepeatingTask {
public:
    TimelineTask(brls::Time period, std::function<void()> callback)
        : brls::RepeatingTask(period), m_callback(std::move(callback)) {}
    void run() override { if (m_callback) m_callback(); }
private:
    std::function<void()> m_callback;
};

PlayerView::PlayerView(PlexServer* server, const plex::MediaItem& item, int bitrate, const std::string& resolution, int mediaIndex)
    : m_server(server), m_item(item), m_bitrate(bitrate), m_resolution(resolution), m_mediaIndex(mediaIndex) {

    s_currentInstance = this;

    setFocusable(true);
    setHideHighlight(true);
    setWidthPercentage(100);
    setHeightPercentage(100);

    m_videoView = new VideoView();
    m_videoView->setWidthPercentage(100);
    m_videoView->setHeightPercentage(100);
    addView(m_videoView);

    setupOsd();

    m_profile = new VideoProfile();
    addView(m_profile);

    initMpv();

    registerAction("Back", brls::BUTTON_B, [this](brls::View*) {
        auto* mpv = MPVCore::getInstance();
        mpv->disableSyncCallbacks();
        mpv->stop();
        brls::Application::popActivity();
        return true;
    });

    registerAction("Toggle Play", brls::BUTTON_A, [this](brls::View*) {
        auto* mpv = MPVCore::getInstance();
        mpv->togglePlay();
        showOsd();
        return true;
    });

    registerAction("Seek Forward", brls::BUTTON_RB, [this](brls::View*) {
        int seekMs = SettingsManager::getInstance()->getSeekIncrement() * 1000;
        if (m_isDirectPlay) {
            MPVCore::getInstance()->seekRelative(seekMs);
        } else {
            seekRelative(seekMs);
        }
        showOsd();
        return true;
    });

    registerAction("Seek Backward", brls::BUTTON_LB, [this](brls::View*) {
        int seekMs = SettingsManager::getInstance()->getSeekIncrement() * 1000;
        if (m_isDirectPlay) {
            MPVCore::getInstance()->seekRelative(-seekMs);
        } else {
            seekRelative(-seekMs);
        }
        showOsd();
        return true;
    });

    registerAction("Toggle OSD", brls::BUTTON_Y, [this](brls::View*) {
        toggleOsd();
        return true;
    });

    registerAction("Cycle Subtitles", brls::BUTTON_X, [this](brls::View*) {
        auto* mpv = MPVCore::getInstance();
        mpv->cycleSubtitles();
        showOsd();
        brls::Application::notify("Subtitles cycled");
        return true;
    });

    registerAction("Cycle Audio", brls::BUTTON_LSB, [this](brls::View*) {
        auto* mpv = MPVCore::getInstance();
        mpv->cycleAudio();
        showOsd();
        brls::Application::notify("Audio track cycled");
        return true;
    });

    registerAction("Toggle Profile", brls::BUTTON_RSB, [this](brls::View*) {
        toggleProfile();
        return true;
    });

    m_osdTimer = new OsdTask(1000, [this]() {
        if (!s_isActive || s_currentInstance != this) return;
        if (m_osdVisible) {
            m_osdTimeout++;
            if (m_osdTimeout >= OSD_TIMEOUT_SECONDS) {
                auto* mpv = MPVCore::getInstance();
                if (mpv->isPlaying()) {
                    hideOsd();
                }
            }
        }
    });
    m_osdTimer->start();

    m_timelineTimer = new TimelineTask(10000, [this]() {
        if (!s_isActive || s_currentInstance != this) return;
        reportTimeline();
    });
    m_timelineTimer->start();
}

PlayerView::~PlayerView() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }

    auto* mpv = MPVCore::getInstance();
    mpv->clearCallbacks();

    if (m_osdTimer) {
        m_osdTimer->stop();
        delete m_osdTimer;
    }
    if (m_timelineTimer) {
        m_timelineTimer->stop();
        delete m_timelineTimer;
    }
}

brls::View* PlayerView::create() {
    return nullptr;
}

void PlayerView::initMpv() {
    auto* mpv = MPVCore::getInstance();
    mpv->enableSyncCallbacks();
    mpv->init();

    mpv->setOnStateChanged([this](PlaybackState state) {
        if (!s_isActive || s_currentInstance != this) return;
        onStateChanged(state);
    });

    mpv->setOnProgress([this](int64_t position, int64_t duration) {
        if (!s_isActive || s_currentInstance != this) return;
        onProgress(position, duration);
    });

    mpv->setOnError([this](const std::string& error) {
        if (!s_isActive || s_currentInstance != this) return;
        onError(error);
    });

    mpv->setOnEndFile([this](bool reachedEof) {
        if (!s_isActive || s_currentInstance != this) return;
        onEndFile(reachedEof);
    });
}

void PlayerView::setupOsd() {
    m_osdContainer = new brls::Box();
    m_osdContainer->setPositionType(brls::PositionType::ABSOLUTE);
    m_osdContainer->setPositionTop(0);
    m_osdContainer->setPositionLeft(0);
    m_osdContainer->setPositionRight(0);
    m_osdContainer->setPositionBottom(0);
    m_osdContainer->setAxis(brls::Axis::COLUMN);
    m_osdContainer->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    m_osdContainer->setFocusable(false);
    addView(m_osdContainer);

    m_topBar = new brls::Box();
    m_topBar->setWidth(brls::View::AUTO);
    m_topBar->setHeight(80);
    m_topBar->setAxis(brls::Axis::ROW);
    m_topBar->setAlignItems(brls::AlignItems::CENTER);
    m_topBar->setPaddingLeft(40);
    m_topBar->setPaddingRight(40);
    m_topBar->setBackgroundColor(nvgRGBA(0, 0, 0, 180));
    m_osdContainer->addView(m_topBar);

    m_titleLabel = new brls::Label();
    std::string displayTitle = m_item.title;
    if (!m_item.editionTitle.empty()) {
        displayTitle += " - " + m_item.editionTitle + " Edition";
    }
    m_titleLabel->setText(displayTitle);
    m_titleLabel->setFontSize(24);
    m_titleLabel->setTextColor(nvgRGB(255, 255, 255));
    m_topBar->addView(m_titleLabel);

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    m_osdContainer->addView(spacer);

    m_bottomBar = new brls::Box();
    m_bottomBar->setWidth(brls::View::AUTO);
    m_bottomBar->setHeight(120);
    m_bottomBar->setAxis(brls::Axis::COLUMN);
    m_bottomBar->setJustifyContent(brls::JustifyContent::CENTER);
    m_bottomBar->setPaddingLeft(40);
    m_bottomBar->setPaddingRight(40);
    m_bottomBar->setPaddingTop(20);
    m_bottomBar->setPaddingBottom(20);
    m_bottomBar->setBackgroundColor(nvgRGBA(0, 0, 0, 180));
    m_osdContainer->addView(m_bottomBar);

    auto* progressRow = new brls::Box();
    progressRow->setWidth(brls::View::AUTO);
    progressRow->setHeight(brls::View::AUTO);
    progressRow->setAxis(brls::Axis::ROW);
    progressRow->setAlignItems(brls::AlignItems::CENTER);
    progressRow->setMarginBottom(10);
    m_bottomBar->addView(progressRow);

    m_progressSlider = new brls::Slider();
    m_progressSlider->setWidth(brls::View::AUTO);
    m_progressSlider->setGrow(1.0f);
    m_progressSlider->setHeight(30);
    m_progressSlider->setFocusable(false);
    m_progressSlider->setProgress(0);
    progressRow->addView(m_progressSlider);

    auto* controlRow = new brls::Box();
    controlRow->setWidth(brls::View::AUTO);
    controlRow->setHeight(brls::View::AUTO);
    controlRow->setAxis(brls::Axis::ROW);
    controlRow->setAlignItems(brls::AlignItems::CENTER);
    controlRow->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    m_bottomBar->addView(controlRow);

    m_stateLabel = new brls::Label();
    m_stateLabel->setText("Loading...");
    m_stateLabel->setFontSize(18);
    m_stateLabel->setTextColor(nvgRGB(255, 255, 255));
    controlRow->addView(m_stateLabel);

    m_timeLabel = new brls::Label();
    m_timeLabel->setText("00:00 / 00:00");
    m_timeLabel->setFontSize(18);
    m_timeLabel->setTextColor(nvgRGB(255, 255, 255));
    controlRow->addView(m_timeLabel);
}

void PlayerView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    s_isActive = true;
    ImageLoader::pause();
    if (SettingsManager::getInstance()->isOverclockEnabled()) {
        SwitchSys::setClock(true);
    }
    startPlayback();
}

void PlayerView::willDisappear(bool resetState) {
    brls::Box::willDisappear(resetState);
    s_isActive = false;
    ImageLoader::resume();
    if (SettingsManager::getInstance()->isOverclockEnabled()) {
        SwitchSys::setClock(false);
    }
    reportStopped();
}

brls::View* PlayerView::getDefaultFocus() {
    return this;
}

void PlayerView::onStateChanged(PlaybackState state) {
    switch (state) {
        case PlaybackState::Stopped:
            m_stateLabel->setText("Stopped");
            break;
        case PlaybackState::Playing:
            m_stateLabel->setText("Playing");
            if (m_pendingSeek) {
                m_pendingSeek = false;
                brls::Logger::info("Seeking to resume position: {}ms", m_startOffset);
                MPVCore::getInstance()->seek(m_startOffset);
            }
            break;
        case PlaybackState::Paused:
            m_stateLabel->setText("Paused");
            showOsd();
            break;
        case PlaybackState::Buffering:
            m_stateLabel->setText("Buffering...");
            showOsd();
            break;
    }
}

void PlayerView::onProgress(int64_t position, int64_t duration) {
    int64_t actualPosition = m_isDirectPlay ? position : (position + m_startOffset);
    int64_t totalDuration = m_item.duration > 0 ? m_item.duration : duration;
    m_lastReportedPosition = actualPosition;
    if (totalDuration > 0) {
        float progress = static_cast<float>(actualPosition) / static_cast<float>(totalDuration);
        m_progressSlider->setProgress(progress);
    }
    m_timeLabel->setText(formatTime(actualPosition) + " / " + formatTime(totalDuration));
}

void PlayerView::onError(const std::string& error) {
    brls::Logger::error("Playback error: {}", error);
    m_stateLabel->setText("Error");
    showOsd();

    brls::Dialog* dialog = new brls::Dialog("Playback Error: " + error);
    dialog->addButton("OK", [this]() {
        brls::Application::popActivity();
    });
    dialog->setCancelable(false);
    dialog->open();
}

std::string PlayerView::formatTime(int64_t ms) {
    int64_t totalSeconds = ms / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    char buffer[16];
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    }
    return std::string(buffer);
}

void PlayerView::showOsd() {
    m_osdVisible = true;
    m_osdTimeout = 0;
    m_osdContainer->setVisibility(brls::Visibility::VISIBLE);
}

void PlayerView::hideOsd() {
    m_osdVisible = false;
    m_osdContainer->setVisibility(brls::Visibility::INVISIBLE);
}

void PlayerView::toggleOsd() {
    if (m_osdVisible) {
        hideOsd();
    } else {
        showOsd();
    }
}

void PlayerView::toggleProfile() {
    if (m_profile->getVisibility() == brls::Visibility::VISIBLE) {
        m_profile->setVisibility(brls::Visibility::INVISIBLE);
    } else {
        int idx = (m_mediaIndex < static_cast<int>(m_item.media.size())) ? m_mediaIndex : 0;
        const plex::Media* media = m_item.media.empty() ? nullptr : &m_item.media[idx];
        m_profile->update(media);
        m_profile->setVisibility(brls::Visibility::VISIBLE);
    }
}

void PlayerView::updateOsd() {
    auto* mpv = MPVCore::getInstance();
    onProgress(mpv->getPosition(), mpv->getDuration());
}

void PlayerView::startPlayback() {
    if (!m_server) {
        onError("No server available");
        return;
    }

    int idx = (m_mediaIndex < static_cast<int>(m_item.media.size())) ? m_mediaIndex : 0;
    if (m_item.media.empty() || m_item.media[idx].parts.empty()) {
        onError("No media available for playback");
        return;
    }

    m_stateLabel->setText("Loading...");
    showOsd();

    bool forceTranscode = (m_bitrate > 0);
    m_startOffset = m_item.viewOffset;
    brls::Logger::info("Starting stream - bitrate: {} kbps, resolution: {}, forceTranscode: {}, offset: {}ms",
        m_bitrate, m_resolution.empty() ? "original" : m_resolution, forceTranscode ? "yes" : "no", m_startOffset);

    PlexApi::getPlaybackDecision(m_server, m_item.ratingKey, m_bitrate, m_resolution, forceTranscode, m_startOffset, m_mediaIndex,
        [this](const plex::PlaybackInfo& info) {
            if (!s_isActive || s_currentInstance != this) return;

            if (info.playbackUrl.empty()) {
                onError("Could not determine playback URL");
                return;
            }

            m_isDirectPlay = (info.protocol == "http");
            m_sessionId = info.sessionId;
            m_pendingSeek = m_isDirectPlay && m_startOffset > 0;

            brls::Logger::info("Starting playback ({}): {}", info.protocol, info.playbackUrl);
            auto* mpv = MPVCore::getInstance();
            int actualBitrate = m_isDirectPlay ?
                (m_item.media.empty() ? 0 : m_item.media[m_mediaIndex].bitrate) : 0;
            mpv->play(info.playbackUrl, m_isDirectPlay, actualBitrate);
        },
        [this](const std::string& error) {
            if (!s_isActive || s_currentInstance != this) return;
            onError("Transcode failed: " + error);
        }
    );
}

void PlayerView::reportTimeline() {
    if (!m_server) return;

    auto* mpv = MPVCore::getInstance();
    if (!mpv->isValid()) return;

    std::string state;
    switch (mpv->getState()) {
        case PlaybackState::Playing:
            state = "playing";
            break;
        case PlaybackState::Paused:
            state = "paused";
            break;
        case PlaybackState::Buffering:
            state = "buffering";
            break;
        default:
            state = "stopped";
            break;
    }

    int64_t actualPosition = m_isDirectPlay ? mpv->getPosition() : (mpv->getPosition() + m_startOffset);
    int64_t totalDuration = m_item.duration > 0 ? m_item.duration : mpv->getDuration();
    m_lastReportedPosition = actualPosition;
    PlexApi::reportTimeline(
        m_server,
        m_item.ratingKey,
        actualPosition,
        state,
        totalDuration,
        m_sessionId
    );
}

void PlayerView::reportStopped() {
    if (!m_server) return;
    PlexApi::reportTimeline(m_server, m_item.ratingKey, m_lastReportedPosition, "stopped", m_item.duration, m_sessionId);
}

void PlayerView::onEndFile(bool reachedEof) {
    if (!reachedEof) return;

    auto* mpv = MPVCore::getInstance();
    auto* settings = SettingsManager::getInstance();
    if (!settings->isAutoPlayNextEnabled()) {
        mpv->disableSyncCallbacks();
        brls::Application::popActivity();
        return;
    }

    if (isEpisode()) {
        playNextEpisode();
    } else {
        mpv->disableSyncCallbacks();
        brls::Application::popActivity();
    }
}

bool PlayerView::isEpisode() const {
    return m_item.mediaType == plex::MediaType::Episode;
}

void PlayerView::playNextEpisode() {
    if (!m_server || m_item.parentRatingKey == 0) {
        MPVCore::getInstance()->disableSyncCallbacks();
        brls::Application::popActivity();
        return;
    }

    int currentIndex = m_item.index;

    PlexApi::getChildren(m_server, m_item.parentRatingKey,
        [this, currentIndex](std::vector<plex::MediaItem> episodes) {
            if (!s_isActive || s_currentInstance != this) return;

            plex::MediaItem nextEpisode;
            bool foundNext = false;

            for (const auto& ep : episodes) {
                if (ep.index == currentIndex + 1) {
                    nextEpisode = ep;
                    foundNext = true;
                    break;
                }
            }

            if (foundNext && !nextEpisode.media.empty()) {
                m_item = nextEpisode;
                std::string nextTitle = m_item.title;
                if (!m_item.editionTitle.empty()) {
                    nextTitle += " - " + m_item.editionTitle + " Edition";
                }
                m_titleLabel->setText(nextTitle);
                brls::Application::notify("Playing next: " + m_item.title);
                startPlayback();
            } else {
                brls::Application::notify("No more episodes");
                MPVCore::getInstance()->disableSyncCallbacks();
                brls::Application::popActivity();
            }
        },
        [this](const std::string& error) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::error("Failed to get next episode: {}", error);
            MPVCore::getInstance()->disableSyncCallbacks();
            brls::Application::popActivity();
        }
    );
}

void PlayerView::seekRelative(int64_t deltaMs) {
    m_pendingSeekDelta += deltaMs;
    showOsd();

    if (!m_seekDebounceActive) {
        m_seekDebounceActive = true;
        brls::delay(500, [this]() {
            if (s_currentInstance == this && s_isActive) {
                executeSeek();
            }
        });
    }
}

void PlayerView::executeSeek() {
    m_seekDebounceActive = false;
    if (m_pendingSeekDelta == 0) return;

    auto* mpv = MPVCore::getInstance();
    int64_t currentPos = mpv->getPosition();
    int64_t accumulatedDelta = m_pendingSeekDelta;
    int64_t newOffset = m_startOffset + currentPos + accumulatedDelta;
    m_pendingSeekDelta = 0;

    if (newOffset < 0) newOffset = 0;
    int64_t duration = m_item.duration;
    if (duration > 0 && newOffset > duration) newOffset = duration - 1000;

    brls::Logger::info("HLS seek: current={}ms, delta={}ms, newOffset={}ms", currentPos, accumulatedDelta, newOffset);

    startPlaybackWithOffset(newOffset);
}

void PlayerView::startPlaybackWithOffset(int64_t offsetMs) {
    if (!m_server) {
        onError("No server available");
        return;
    }

    auto* mpv = MPVCore::getInstance();
    mpv->stop();

    m_stateLabel->setText("Seeking...");
    showOsd();

    bool forceTranscode = (m_bitrate > 0);
    m_startOffset = offsetMs;

    brls::Logger::info("Restarting stream at offset {}ms", offsetMs);

    PlexApi::getPlaybackDecision(m_server, m_item.ratingKey, m_bitrate, m_resolution, forceTranscode, m_startOffset, m_mediaIndex,
        [this](const plex::PlaybackInfo& info) {
            if (!s_isActive || s_currentInstance != this) return;

            if (info.playbackUrl.empty()) {
                onError("Could not determine playback URL");
                return;
            }

            m_isDirectPlay = (info.protocol == "http");
            m_sessionId = info.sessionId;
            m_pendingSeek = false;

            brls::Logger::info("Restarting playback ({}): {}", info.protocol, info.playbackUrl);
            auto* mpv = MPVCore::getInstance();
            int actualBitrate = m_isDirectPlay ?
                (m_item.media.empty() ? 0 : m_item.media[m_mediaIndex].bitrate) : 0;
            mpv->play(info.playbackUrl, m_isDirectPlay, actualBitrate);
        },
        [this](const std::string& error) {
            if (!s_isActive || s_currentInstance != this) return;
            onError("Seek failed: " + error);
        }
    );
}
