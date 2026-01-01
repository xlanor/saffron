#ifndef SAFFRON_COLLECTIONS_TAB_HPP
#define SAFFRON_COLLECTIONS_TAB_HPP

#include <borealis.hpp>
#include <vector>

#include "models/plex_types.hpp"

class PlexServer;
class SettingsManager;

class CollectionItemView;

class CollectionsTab : public brls::Box {
public:
    CollectionsTab();
    ~CollectionsTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    void loadCollections();

    BRLS_BIND(brls::Box, collectionContainer, "collections/container");
    BRLS_BIND(brls::Box, emptyMessage, "collections/empty");
    BRLS_BIND(brls::Label, serverLabel, "collections/serverLabel");
    BRLS_BIND(brls::Box, spinnerContainer, "collections/spinner");

    SettingsManager* m_settings = nullptr;
    std::vector<CollectionItemView*> m_items;

    static CollectionsTab* s_currentInstance;
    static bool s_isActive;
};

class CollectionItemView : public brls::Box {
public:
    CollectionItemView(PlexServer* server, const plex::Collection& collection);

    void cancelPendingImage();
    void onFocusGained() override;
    void onFocusLost() override;

private:
    PlexServer* m_server = nullptr;
    plex::Collection m_collection;

    brls::Image* m_thumbImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_countLabel = nullptr;

    NVGcolor m_originalTitleColor;
    NVGcolor m_originalCountColor;
};

#endif
