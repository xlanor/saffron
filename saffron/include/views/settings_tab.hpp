#ifndef SAFFRON_SETTINGS_TAB_HPP
#define SAFFRON_SETTINGS_TAB_HPP

#include <borealis.hpp>
#include <chrono>

class SettingsTab : public brls::Box {
public:
    SettingsTab();
    ~SettingsTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;

private:
    void setupUI();
    void createPlaybackSection(brls::Box* container);
    void createPowerUserSection(brls::Box* container);
    void createVersionLabel(brls::Box* container);
    void updatePowerUserVisibility();

    brls::BooleanCell* m_autoPlayCell = nullptr;
    brls::BooleanCell* m_overclockCell = nullptr;
    brls::BooleanCell* m_bufferBeforePlayCell = nullptr;
    brls::SelectorCell* m_seekSelector = nullptr;
    brls::SelectorCell* m_cacheSelector = nullptr;
    brls::SelectorCell* m_videoSyncSelector = nullptr;
    brls::SelectorCell* m_framebufferSelector = nullptr;

    brls::Box* m_powerUserSection = nullptr;
    brls::Label* m_versionLabel = nullptr;
    int m_powerUserClickCount = 0;
    std::chrono::steady_clock::time_point m_lastPowerUserClick;
};

#endif
