#ifndef SAFFRON_SHOW_DETAIL_VIEW_HPP
#define SAFFRON_SHOW_DETAIL_VIEW_HPP

#include <borealis.hpp>
#include <vector>
#include "models/plex_types.hpp"

class PlexServer;
class RecyclingGrid;

class ShowDetailView : public brls::Box {
public:
    ShowDetailView(PlexServer* server, const plex::MediaItem& show);
    ~ShowDetailView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    brls::View* getDefaultFocus() override;
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    void setupUI();
    void updateUI();
    void loadFullMetadata();
    void loadSeasons();
    void showSeasonDropdown();
    void onSeasonSelected(const plex::MediaItem& season);
    void setupCastSection(brls::Box* parent);
    std::string formatDuration(int64_t ms);

    PlexServer* m_server = nullptr;
    plex::MediaItem m_show;
    std::vector<plex::MediaItem> m_seasons;
    bool m_seasonsLoaded = false;
    bool m_metadataLoaded = false;
    bool m_isVisible = false;
    int m_metadataRequestId = 0;
    int m_seasonsRequestId = 0;

    brls::Image* m_backdropImage = nullptr;
    brls::Image* m_posterImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_yearLabel = nullptr;
    brls::Label* m_ratingLabel = nullptr;
    brls::Label* m_durationLabel = nullptr;
    brls::Label* m_studioLabel = nullptr;
    brls::Label* m_episodeCountLabel = nullptr;
    brls::Label* m_taglineLabel = nullptr;
    brls::Label* m_summaryLabel = nullptr;
    brls::Button* m_seasonButton = nullptr;
    brls::HScrollingFrame* m_genresScrollFrame = nullptr;
    brls::Box* m_genresRow = nullptr;
    brls::Box* m_castSection = nullptr;
    RecyclingGrid* m_castGrid = nullptr;

    static ShowDetailView* s_instance;
};

#endif
