#ifndef SAFFRON_LIBRARY_BROWSE_VIEW_HPP
#define SAFFRON_LIBRARY_BROWSE_VIEW_HPP

#include <borealis.hpp>
#include "models/plex_types.hpp"
#include "views/media_grid_view.hpp"

class PlexServer;

class LibraryBrowseView : public brls::Box {
public:
    LibraryBrowseView(PlexServer* server, const plex::Library& library);
    ~LibraryBrowseView() override;

    void willAppear(bool resetState) override;

private:
    void loadItems();
    void loadMoreItems();

    PlexServer* m_server = nullptr;
    plex::Library m_library;

    brls::Label* m_titleLabel = nullptr;
    brls::ScrollingFrame* m_scrollView = nullptr;
    MediaGridView* m_gridView = nullptr;
    brls::Box* m_loadingBox = nullptr;
};

#endif
