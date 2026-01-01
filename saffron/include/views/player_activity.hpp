#ifndef SAFFRON_PLAYER_ACTIVITY_HPP
#define SAFFRON_PLAYER_ACTIVITY_HPP

#include <borealis.hpp>
#include "views/player_view.hpp"
#include "core/plex_server.hpp"
#include "models/plex_types.hpp"

class PlayerActivity : public brls::Activity {
public:
    PlayerActivity(PlexServer* server, const plex::MediaItem& item,
                   int bitrate = 0, const std::string& resolution = "",
                   int mediaIndex = 0);

    brls::View* createContentView() override;

private:
    PlexServer* m_server;
    plex::MediaItem m_item;
    int m_bitrate;
    std::string m_resolution;
    int m_mediaIndex;
};

#endif
