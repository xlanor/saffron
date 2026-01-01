#ifndef SAFFRON_SEASON_VIEW_HPP
#define SAFFRON_SEASON_VIEW_HPP

#include <borealis.hpp>
#include <vector>
#include "models/plex_types.hpp"

class PlexServer;

class SeasonView : public brls::Box {
public:
    SeasonView(PlexServer* server, const plex::MediaItem& season);
    ~SeasonView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    brls::View* getDefaultFocus() override;

private:
    void setupUI();
    void loadEpisodes();
    void onEpisodeSelected(const plex::MediaItem& episode);

    PlexServer* m_server = nullptr;
    plex::MediaItem m_season;
    std::vector<plex::MediaItem> m_episodes;
    int m_requestId = 0;
    bool m_episodesLoaded = false;
    bool m_isVisible = false;

    brls::Label* m_headerLabel = nullptr;
    brls::Box* m_episodesList = nullptr;
    std::vector<brls::Image*> m_episodeThumbs;

    static SeasonView* s_instance;
};

#endif
