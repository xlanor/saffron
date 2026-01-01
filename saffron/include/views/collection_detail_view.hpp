#ifndef SAFFRON_COLLECTION_DETAIL_VIEW_HPP
#define SAFFRON_COLLECTION_DETAIL_VIEW_HPP

#include <borealis.hpp>
#include "models/plex_types.hpp"

class PlexServer;
class RecyclingGrid;
class MediaCardDataSource;

class CollectionDetailView : public brls::Box {
public:
    CollectionDetailView(PlexServer* server, const plex::Collection& collection);
    ~CollectionDetailView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    brls::View* getDefaultFocus() override;

private:
    void setupUI();
    void updateUI();
    void loadCollectionItems();

    PlexServer* m_server = nullptr;
    plex::Collection m_collection;

    brls::Image* m_posterImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_countLabel = nullptr;
    brls::Label* m_summaryLabel = nullptr;

    RecyclingGrid* m_grid = nullptr;
    MediaCardDataSource* m_dataSource = nullptr;

    bool m_isVisible = false;
    bool m_itemsLoaded = false;
    int m_requestId = 0;

    static CollectionDetailView* s_instance;
};

#endif
