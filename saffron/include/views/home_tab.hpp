#ifndef SAFFRON_HOME_TAB_HPP
#define SAFFRON_HOME_TAB_HPP

#include <borealis.hpp>
#include <vector>
#include <memory>

#include "models/plex_types.hpp"

class PlexServer;
class SettingsManager;
class HubRowView;

class HomeTab : public brls::Box {
public:
    HomeTab();
    ~HomeTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    void loadHubs();
    void clearHubs();
    void cancelPendingImages();

    BRLS_BIND(brls::Box, hubContainer, "home/container");
    BRLS_BIND(brls::Box, emptyMessage, "home/empty");
    BRLS_BIND(brls::Box, spinnerContainer, "home/spinner");

    SettingsManager* m_settings = nullptr;
    std::vector<HubRowView*> m_hubRows;

    static HomeTab* s_currentInstance;
    static bool s_isActive;
};

class HubRowView : public brls::Box {
public:
    HubRowView(PlexServer* server, const plex::Hub& hub);

    void setOnItemClick(std::function<void(const plex::MediaItem&)> callback);
    void cancelPendingImages();

private:
    PlexServer* m_server = nullptr;
    plex::Hub m_hub;

    brls::Label* m_titleLabel = nullptr;
    brls::HScrollingFrame* m_scrollFrame = nullptr;
    brls::Box* m_itemsContainer = nullptr;

    std::function<void(const plex::MediaItem&)> m_onItemClick;

    void addItems();
};

class HubItemView : public brls::Box {
public:
    HubItemView(PlexServer* server, const plex::MediaItem& item);

    void setOnClick(std::function<void()> callback);
    void cancelPendingImage();
    const plex::MediaItem& getItem() const { return m_item; }

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    void onFocusGained() override;
    void onFocusLost() override;

private:
    PlexServer* m_server = nullptr;
    plex::MediaItem m_item;

    brls::Image* m_image = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_subtitleLabel = nullptr;
    NVGcolor m_originalTitleColor;
    NVGcolor m_originalSubtitleColor;

    void loadImage();
};

#endif
