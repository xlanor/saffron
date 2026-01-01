#include "views/settings_tab.hpp"
#include "core/settings_manager.hpp"
#include <fstream>

SettingsTab::SettingsTab() {
    setupUI();
}

SettingsTab::~SettingsTab() {}

brls::View* SettingsTab::create() {
    return new SettingsTab();
}

void SettingsTab::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
}

void SettingsTab::setupUI() {
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setPaddingTop(30);
    setPaddingLeft(40);
    setPaddingRight(40);
    setPaddingBottom(30);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setWidth(brls::View::AUTO);
    scroll->setHeight(brls::View::AUTO);
    scroll->setGrow(1.0f);
    addView(scroll);

    auto* container = new brls::Box();
    container->setAxis(brls::Axis::COLUMN);
    container->setWidth(brls::View::AUTO);
    container->setHeight(brls::View::AUTO);
    scroll->setContentView(container);

    createPlaybackSection(container);
    createPowerUserSection(container);
    createVersionLabel(container);
}

void SettingsTab::createPlaybackSection(brls::Box* container) {
    auto* header = new brls::Header();
    header->setTitle("Playback");
    header->setFocusable(false);
    container->addView(header);

    auto* settings = SettingsManager::getInstance();

    m_autoPlayCell = new brls::BooleanCell();
    m_autoPlayCell->title->setText("Auto-Play Next");
    m_autoPlayCell->detail->setText("Automatically play the next episode");
    m_autoPlayCell->setOn(settings->isAutoPlayNextEnabled());
    m_autoPlayCell->getEvent()->subscribe([this, settings](bool on) {
        settings->setAutoPlayNext(on);
        settings->writeFile();
    });
    container->addView(m_autoPlayCell);

    m_seekSelector = new brls::SelectorCell();
    m_seekSelector->title->setText("Seek Increment");
    m_seekSelector->setData({"5 seconds", "10 seconds", "15 seconds", "30 seconds", "45 seconds", "60 seconds"});

    int seekIndex = 1;
    int seekVal = settings->getSeekIncrement();
    if (seekVal == 5) seekIndex = 0;
    else if (seekVal == 10) seekIndex = 1;
    else if (seekVal == 15) seekIndex = 2;
    else if (seekVal == 30) seekIndex = 3;
    else if (seekVal == 45) seekIndex = 4;
    else if (seekVal == 60) seekIndex = 5;
    m_seekSelector->setSelection(seekIndex);

    m_seekSelector->getEvent()->subscribe([this, settings](int selected) {
        int values[] = {5, 10, 15, 30, 45, 60};
        settings->setSeekIncrement(values[selected]);
        settings->writeFile();
    });
    container->addView(m_seekSelector);

    m_cacheSelector = new brls::SelectorCell();
    m_cacheSelector->title->setText("In Memory Cache");
    m_cacheSelector->setData({"Disabled", "10 MB", "20 MB", "50 MB", "100 MB", "200 MB", "500 MB"});

    int cacheIndex = 3;
    int cacheVal = settings->getInMemoryCache();
    if (cacheVal == 0) cacheIndex = 0;
    else if (cacheVal == 10) cacheIndex = 1;
    else if (cacheVal == 20) cacheIndex = 2;
    else if (cacheVal == 50) cacheIndex = 3;
    else if (cacheVal == 100) cacheIndex = 4;
    else if (cacheVal == 200) cacheIndex = 5;
    else if (cacheVal == 500) cacheIndex = 6;
    m_cacheSelector->setSelection(cacheIndex);

    m_cacheSelector->getEvent()->subscribe([this, settings](int selected) {
        int values[] = {0, 10, 20, 50, 100, 200, 500};
        settings->setInMemoryCache(values[selected]);
        settings->writeFile();
    });
    container->addView(m_cacheSelector);
}

void SettingsTab::createPowerUserSection(brls::Box* container) {
    auto* settings = SettingsManager::getInstance();

    m_powerUserSection = new brls::Box();
    m_powerUserSection->setAxis(brls::Axis::COLUMN);
    m_powerUserSection->setWidthPercentage(100);
    m_powerUserSection->setVisibility(brls::Visibility::GONE);
    container->addView(m_powerUserSection);

    auto* header = new brls::Header();
    header->setTitle("Power User");
    header->setFocusable(false);
    m_powerUserSection->addView(header);

    m_overclockCell = new brls::BooleanCell();
    m_overclockCell->title->setText("Hardware Boost");
    m_overclockCell->detail->setText("Overclock CPU/GPU during video playback for 4K");
    m_overclockCell->setOn(settings->isOverclockEnabled());
    m_overclockCell->getEvent()->subscribe([settings](bool on) {
        settings->setOverclockEnabled(on);
        settings->writeFile();
    });
    m_powerUserSection->addView(m_overclockCell);

    auto* warningLabel = new brls::Label();
    warningLabel->setText("Warning: Please read the source code to understand what this does!");
    warningLabel->setFontSize(14);
    warningLabel->setTextColor(nvgRGBA(255, 100, 100, 255));
    warningLabel->setMarginLeft(15);
    warningLabel->setMarginBottom(15);
    warningLabel->setFocusable(false);
    m_powerUserSection->addView(warningLabel);

    m_bufferBeforePlayCell = new brls::BooleanCell();
    m_bufferBeforePlayCell->title->setText("Buffer Before Playback");
    m_bufferBeforePlayCell->detail->setText("Wait for buffer to fill before starting 4K direct play");
    m_bufferBeforePlayCell->setOn(settings->isBufferBeforePlayEnabled());
    m_bufferBeforePlayCell->getEvent()->subscribe([settings](bool on) {
        settings->setBufferBeforePlay(on);
        settings->writeFile();
    });
    m_powerUserSection->addView(m_bufferBeforePlayCell);

    auto* mpvHeader = new brls::Header();
    mpvHeader->setTitle("Debugging MPV Options");
    mpvHeader->setFocusable(false);
    m_powerUserSection->addView(mpvHeader);

    m_videoSyncSelector = new brls::SelectorCell();
    m_videoSyncSelector->title->setText("Video Sync Mode");
    m_videoSyncSelector->detail->setText("Controls frame timing synchronization");
    std::vector<std::string> syncModes = {"Audio (Recommended)", "Display Resample"};
    m_videoSyncSelector->setData(syncModes);

    std::string currentMode = settings->getVideoSyncMode();
    m_videoSyncSelector->setSelection(currentMode == "display-resample" ? 1 : 0);

    m_videoSyncSelector->getEvent()->subscribe([settings](int selected) {
        settings->setVideoSyncMode(selected == 0 ? "audio" : "display-resample");
        settings->writeFile();
    });
    m_powerUserSection->addView(m_videoSyncSelector);

    m_framebufferSelector = new brls::SelectorCell();
    m_framebufferSelector->title->setText("Framebuffer Count");
    m_framebufferSelector->detail->setText("Requires restart to take effect");
    std::vector<std::string> fbCounts = {"2", "3", "4"};
    m_framebufferSelector->setData(fbCounts);

    int fbCount = settings->getFramebufferCount();
    m_framebufferSelector->setSelection(fbCount - 2);

    m_framebufferSelector->getEvent()->subscribe([settings](int selected) {
        settings->setFramebufferCount(selected + 2);
        settings->writeFile();
    });
    m_powerUserSection->addView(m_framebufferSelector);

    updatePowerUserVisibility();
}

void SettingsTab::createVersionLabel(brls::Box* container) {
    auto* versionBox = new brls::Box();
    versionBox->setAxis(brls::Axis::ROW);
    versionBox->setJustifyContent(brls::JustifyContent::CENTER);
    versionBox->setAlignItems(brls::AlignItems::CENTER);
    versionBox->setWidthPercentage(100);
    versionBox->setHeight(40);
    versionBox->setMarginTop(30);
    versionBox->setFocusable(true);
    versionBox->setHideHighlightBackground(true);
    container->addView(versionBox);

    m_versionLabel = new brls::Label();
    std::ifstream versionFile("romfs:/version.txt");
    std::string version = "Unknown";
    if (versionFile.is_open()) {
        std::getline(versionFile, version);
    }
    m_versionLabel->setText("Version " + version);
    m_versionLabel->setFontSize(14);
    m_versionLabel->setTextColor(nvgRGBA(100, 100, 100, 255));
    versionBox->addView(m_versionLabel);

    versionBox->registerClickAction([this](brls::View*) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastPowerUserClick).count();

        if (elapsed > 3000) {
            m_powerUserClickCount = 0;
        }

        m_lastPowerUserClick = now;
        m_powerUserClickCount++;

        auto* settings = SettingsManager::getInstance();
        if (m_powerUserClickCount >= 7 && !settings->isPowerUserMenuUnlocked()) {
            settings->setPowerUserMenuUnlocked(true);
            settings->writeFile();
            brls::Application::notify("Power User Menu unlocked!");
            updatePowerUserVisibility();
            m_powerUserClickCount = 0;
        }
        return true;
    });
}

void SettingsTab::updatePowerUserVisibility() {
    if (m_powerUserSection) {
        auto* settings = SettingsManager::getInstance();
        m_powerUserSection->setVisibility(
            settings->isPowerUserMenuUnlocked() ?
            brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

