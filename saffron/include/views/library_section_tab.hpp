#ifndef SAFFRON_LIBRARY_SECTION_TAB_HPP
#define SAFFRON_LIBRARY_SECTION_TAB_HPP

#include <borealis.hpp>
#include <unordered_map>
#include "models/plex_types.hpp"

class PlexServer;
class RecyclingGrid;
class MediaCardDataSource;

struct LibraryCacheEntry {
    std::vector<plex::MediaItem> items;
    int totalSize = 0;
};

class LibrarySectionTab : public brls::Box {
public:
    LibrarySectionTab(PlexServer* server, const plex::Library& library);
    ~LibrarySectionTab() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    const plex::Library& getLibrary() const { return m_library; }

    static void clearCache();

private:
    static constexpr int PAGE_SIZE = 20;

    PlexServer* m_server = nullptr;
    plex::Library m_library;
    RecyclingGrid* m_grid = nullptr;
    MediaCardDataSource* m_dataSource = nullptr;
    brls::Box* m_spinnerContainer = nullptr;
    int m_totalItems = 0;

    void loadItems();
    void loadMoreItems();
    void restoreFromCache();

    static std::unordered_map<int, LibraryCacheEntry> s_cache;
    static LibrarySectionTab* s_currentInstance;
    static bool s_isActive;
};

#endif
