#include "views/player_activity.hpp"

PlayerActivity::PlayerActivity(PlexServer* server, const plex::MediaItem& item, int bitrate, const std::string& resolution, int mediaIndex)
    : m_server(server), m_item(item), m_bitrate(bitrate), m_resolution(resolution), m_mediaIndex(mediaIndex) {
}

brls::View* PlayerActivity::createContentView() {
    return new PlayerView(m_server, m_item, m_bitrate, m_resolution, m_mediaIndex);
}
