#ifndef SAFFRON_PLAYLISTS_TAB_HPP
#define SAFFRON_PLAYLISTS_TAB_HPP

#include <borealis.hpp>
#include <vector>

#include "models/plex_types.hpp"

class PlexServer;
class SettingsManager;

class PlaylistItemView;

class PlaylistsTab : public brls::Box {
public:
    PlaylistsTab();
    ~PlaylistsTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    void loadPlaylists();

    BRLS_BIND(brls::Box, playlistContainer, "playlists/container");
    BRLS_BIND(brls::Box, emptyMessage, "playlists/empty");
    BRLS_BIND(brls::Label, serverLabel, "playlists/serverLabel");
    BRLS_BIND(brls::Box, spinnerContainer, "playlists/spinner");

    SettingsManager* m_settings = nullptr;
    std::vector<PlaylistItemView*> m_items;

    static PlaylistsTab* s_currentInstance;
    static bool s_isActive;
};

class PlaylistItemView : public brls::Box {
public:
    PlaylistItemView(PlexServer* server, const plex::Playlist& playlist);

    void cancelPendingImage();

private:
    PlexServer* m_server = nullptr;
    plex::Playlist m_playlist;

    brls::Image* m_thumbImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_countLabel = nullptr;
    brls::Label* m_durationLabel = nullptr;
};

#endif
