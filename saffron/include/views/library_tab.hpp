#ifndef SAFFRON_LIBRARY_TAB_HPP
#define SAFFRON_LIBRARY_TAB_HPP

#include <borealis.hpp>
#include <vector>

#include "models/plex_types.hpp"

class PlexServer;
class SettingsManager;
class MediaGridView;

class LibraryTab : public brls::Box {
public:
    LibraryTab();
    ~LibraryTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    BRLS_BIND(brls::SelectorCell, librarySelector, "library/selector");
    BRLS_BIND(brls::Box, contentContainer, "library/content");
    BRLS_BIND(brls::Box, emptyMessage, "library/empty");
    BRLS_BIND(brls::Label, serverLabel, "library/serverLabel");

    SettingsManager* m_settings = nullptr;
    PlexServer* m_server = nullptr;
    MediaGridView* m_gridView = nullptr;

    std::vector<plex::Library> m_libraries;
    int m_selectedLibraryIndex = -1;

    static constexpr int PAGE_SIZE = 30;

    void loadLibraries();
    void onLibrarySelected(int index);
    void loadLibraryContent(const plex::Library& library);
    void loadMoreItems();

    static LibraryTab* s_currentInstance;
    static bool s_isActive;
};

#endif
