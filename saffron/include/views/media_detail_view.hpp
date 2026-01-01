#ifndef SAFFRON_MEDIA_DETAIL_VIEW_HPP
#define SAFFRON_MEDIA_DETAIL_VIEW_HPP

#include <borealis.hpp>
#include <memory>
#include "models/plex_types.hpp"

class PlexServer;
class RecyclingGrid;

class MediaDetailView : public brls::Box {
public:
    MediaDetailView(PlexServer* server, const plex::MediaItem& item);
    ~MediaDetailView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    brls::View* getDefaultFocus() override;

private:
    void loadFullMetadata();
    void setupUI();
    void updateUI();
    void updateTechnicalInfo();
    void playMedia();
    void showResumeDialog();
    void startPlaybackFlow();
    void showQualityMenu(const plex::PlaybackInfo& info);
    void showSourceSelector();
    void showMediaInfo();
    void setupCastSection(brls::Box* parent);
    std::string formatDuration(int64_t ms);
    std::string formatProgress();

    PlexServer* m_server = nullptr;
    plex::MediaItem m_item;
    int m_selectedMediaIndex = 0;

    brls::Image* m_backdropImage = nullptr;
    brls::Image* m_posterImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Box* m_resolutionBadge = nullptr;
    brls::Label* m_resolutionLabel = nullptr;
    brls::Label* m_yearLabel = nullptr;
    brls::Label* m_ratingLabel = nullptr;
    brls::Label* m_durationLabel = nullptr;
    brls::Label* m_studioLabel = nullptr;
    brls::Label* m_taglineLabel = nullptr;
    brls::Label* m_summaryLabel = nullptr;
    brls::Button* m_playButton = nullptr;
    brls::Button* m_sourceButton = nullptr;
    brls::Box* m_progressBar = nullptr;
    brls::Box* m_progressFill = nullptr;
    brls::Label* m_progressLabel = nullptr;
    brls::Label* m_videoInfoLabel = nullptr;
    brls::Label* m_audioInfoLabel = nullptr;
    brls::Label* m_fileInfoLabel = nullptr;
    brls::HScrollingFrame* m_genresScrollFrame = nullptr;
    brls::Box* m_genresRow = nullptr;
    brls::Box* m_castSection = nullptr;
    RecyclingGrid* m_castGrid = nullptr;
    bool m_metadataLoaded = false;
    bool m_isVisible = false;
    int m_requestId = 0;

    static MediaDetailView* s_instance;
};

#endif
