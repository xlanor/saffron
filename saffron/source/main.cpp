// Early SSL initialization - must run before FFmpeg static constructors
// register TLS protocols. Priority 101 runs before default (65535).
#include <switch.h>

__attribute__((constructor(101)))
static void early_ssl_init() {
    sslInitialize(0x3);
    csrngInitialize();
}

__attribute__((destructor(101)))
static void early_ssl_cleanup() {
    csrngExit();
    sslExit();
}

#include <borealis.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/thread_pool.hpp>
#include <borealis/core/video.hpp>
#include <curl/curl.h>
#include <fstream>

#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"
#include "core/mpv_core.hpp"
#include "core/plex_server.hpp"
#include "util/image_loader.hpp"
#include "util/overclock.hpp"
#include "views/home_tab.hpp"
#include "views/search_tab.hpp"
#include "views/library_tab.hpp"

#include "views/library_section_tab.hpp"
#include "views/collections_tab.hpp"
#include "views/playlists_tab.hpp"
#include "views/server_list_tab.hpp"
#include "views/settings_tab.hpp"
#include "views/config_view_tab.hpp"

static std::string getAppVersion() {
    std::ifstream file("romfs:/version.txt");
    if (!file.is_open()) {
        return "";
    }
    std::string version;
    std::getline(file, version);
    return version;
}

void initCustomTheme() {
    brls::Theme::getLightTheme().addColor("color/card", nvgRGB(245, 246, 247));
    brls::Theme::getDarkTheme().addColor("color/card", nvgRGB(51, 52, 53));

    brls::Theme::getLightTheme().addColor("plex/button/background", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("plex/button/background", nvgRGB(229, 160, 13));
    brls::Theme::getLightTheme().addColor("plex/button/background_disabled", nvgRGB(115, 80, 6));
    brls::Theme::getDarkTheme().addColor("plex/button/background_disabled", nvgRGB(115, 80, 6));
    brls::Theme::getLightTheme().addColor("plex/button/text", nvgRGB(0, 0, 0));
    brls::Theme::getDarkTheme().addColor("plex/button/text", nvgRGB(0, 0, 0));
    brls::Theme::getLightTheme().addColor("plex/button/text_disabled", nvgRGB(60, 60, 60));
    brls::Theme::getDarkTheme().addColor("plex/button/text_disabled", nvgRGB(60, 60, 60));

    brls::Theme::getLightTheme().addColor("brls/highlight/color1", nvgRGB(229, 160, 13));
    brls::Theme::getLightTheme().addColor("brls/highlight/color2", nvgRGB(255, 200, 80));
    brls::Theme::getLightTheme().addColor("brls/click_pulse", nvgRGBA(229, 160, 13, 50));
    brls::Theme::getDarkTheme().addColor("brls/highlight/color1", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/highlight/color2", nvgRGB(255, 200, 80));
    brls::Theme::getDarkTheme().addColor("brls/click_pulse", nvgRGBA(229, 160, 13, 50));

    brls::Theme::getLightTheme().addColor("brls/sidebar/active_item", nvgRGB(180, 120, 10));
    brls::Theme::getDarkTheme().addColor("brls/sidebar/active_item", nvgRGB(180, 120, 10));

    brls::Theme::getLightTheme().addColor("brls/accent", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/accent", nvgRGB(229, 160, 13));
    brls::Theme::getLightTheme().addColor("brls/button/primary_enabled_background", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/button/primary_enabled_background", nvgRGB(229, 160, 13));
    brls::Theme::getLightTheme().addColor("brls/slider/line_filled", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/slider/line_filled", nvgRGB(229, 160, 13));

    brls::Theme::getLightTheme().addColor("brls/button/highlight_enabled_text", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/button/highlight_enabled_text", nvgRGB(229, 160, 13));
    brls::Theme::getLightTheme().addColor("brls/button/highlight_disabled_text", nvgRGB(180, 120, 10));
    brls::Theme::getDarkTheme().addColor("brls/button/highlight_disabled_text", nvgRGB(180, 120, 10));
    brls::Theme::getLightTheme().addColor("brls/list/listItem_value_color", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/list/listItem_value_color", nvgRGB(229, 160, 13));

    brls::Theme::getLightTheme().addColor("brls/scrollbar", nvgRGB(229, 160, 13));
    brls::Theme::getDarkTheme().addColor("brls/scrollbar", nvgRGB(229, 160, 13));

    brls::Theme::getLightTheme().addColor("brls/text/focused", nvgRGB(0, 0, 0));
    brls::Theme::getDarkTheme().addColor("brls/text/focused", nvgRGB(0, 0, 0));

    brls::Theme::getLightTheme().addColor("brls/highlight/background", nvgRGBA(229, 160, 13, 255));
    brls::Theme::getDarkTheme().addColor("brls/highlight/background", nvgRGBA(229, 160, 13, 255));
}

class MainActivity;
static MainActivity* s_mainActivityInstance = nullptr;
static bool s_mainActivityActive = false;

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    MainActivity() {
        s_mainActivityInstance = this;
        s_mainActivityActive = true;
    }

    ~MainActivity() {
        if (s_mainActivityInstance == this) {
            s_mainActivityInstance = nullptr;
        }
        s_mainActivityActive = false;
    }

    void onContentAvailable() override {
        brls::Logger::info("MainActivity: onContentAvailable");
        customizeHeader();
        loadLibraryTabs();

        SettingsManager::getInstance()->setOnServerChanged([this]() {
            if (s_mainActivityActive && s_mainActivityInstance == this) {
                loadLibraryTabs();
                updateServerLabel();
            }
        });
    }

private:
    brls::TabFrame* m_tabFrame = nullptr;
    brls::Label* m_serverLabel = nullptr;

    void customizeHeader() {
        auto* appletFrame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
        if (!appletFrame) return;

        std::string version = getAppVersion();
        if (!version.empty()) {
            appletFrame->setTitle("Saffron v" + version);
        }

        appletFrame->setIcon("romfs:/img/icon.jpg");

        auto* header = appletFrame->getHeader();
        if (header && !m_serverLabel) {
            auto* spacer = new brls::Box();
            spacer->setGrow(1.0f);
            header->addView(spacer);

            m_serverLabel = new brls::Label();
            m_serverLabel->setFontSize(18);
            m_serverLabel->setTextColor(nvgRGB(180, 120, 10));
            m_serverLabel->setMarginRight(30);
            header->addView(m_serverLabel);

            updateServerLabel();
        }
    }

    void updateServerLabel() {
        if (!m_serverLabel) return;
        PlexServer* server = SettingsManager::getInstance()->getCurrentServer();
        if (server) {
            m_serverLabel->setText("Connected to: " + server->getName());
        } else {
            m_serverLabel->setText("");
        }
    }

    void loadLibraryTabs() {
        auto* appletFrame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
        if (!appletFrame) {
            brls::Logger::error("MainActivity: Failed to get AppletFrame");
            return;
        }

        m_tabFrame = dynamic_cast<brls::TabFrame*>(appletFrame->getContentView());
        if (!m_tabFrame) {
            brls::Logger::error("MainActivity: Failed to get TabFrame");
            return;
        }

        PlexServer* server = SettingsManager::getInstance()->getCurrentServer();
        if (!server) {
            brls::Logger::info("MainActivity: No server selected, building static tabs only");
            buildStaticTabs();
            return;
        }

        brls::Logger::info("MainActivity: Loading libraries from server '{}'", server->getName());

        PlexApi::getLibrarySections(
            server,
            [this, server](std::vector<plex::Library> libraries) {
                if (!s_mainActivityActive || s_mainActivityInstance != this) return;

                brls::Logger::info("MainActivity: Got {} libraries", libraries.size());

                m_tabFrame->clearTabs();

                m_tabFrame->addTab("Home", []() { return new HomeTab(); });
                m_tabFrame->addTab("Search", []() { return new SearchTab(); });

                for (const auto& lib : libraries) {
                    if (lib.type != "movie" && lib.type != "show") {
                        continue;
                    }

                    brls::Logger::info("MainActivity: Adding tab for '{}'", lib.title);

                    plex::Library libCopy = lib;
                    PlexServer* serverPtr = server;

                    m_tabFrame->addTab(lib.title, [serverPtr, libCopy]() {
                        return new LibrarySectionTab(serverPtr, libCopy);
                    });
                }

                m_tabFrame->addTab("Collections", []() { return new CollectionsTab(); });

                m_tabFrame->addSeparator();
                m_tabFrame->addTab("Servers", []() { return new ServerListTab(); });
                m_tabFrame->addTab("Settings", []() { return new SettingsTab(); });
                m_tabFrame->addTab("Config", []() { return new ConfigViewTab(); });

                m_tabFrame->focusTab(0);
            },
            [this](const std::string& error) {
                if (!s_mainActivityActive || s_mainActivityInstance != this) return;

                brls::Logger::error("MainActivity: Failed to load libraries: {}", error);
                buildStaticTabs();
            }
        );
    }

    void buildStaticTabs() {
        if (!m_tabFrame) return;

        m_tabFrame->clearTabs();
        m_tabFrame->addTab("Home", []() { return new HomeTab(); });
        m_tabFrame->addTab("Search", []() { return new SearchTab(); });
        m_tabFrame->addTab("Collections", []() { return new CollectionsTab(); });
        m_tabFrame->addSeparator();
        m_tabFrame->addTab("Servers", []() { return new ServerListTab(); });
        m_tabFrame->addTab("Settings", []() { return new SettingsTab(); });
        m_tabFrame->addTab("Config", []() { return new ConfigViewTab(); });
        m_tabFrame->focusTab(0);
    }
};

int main(int argc, char* argv[]) {
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);

    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_EN_US;
    VideoContext::highPriorityQueue = false;
    VideoContext::framebufferCount = SettingsManager::loadEarlyFramebufferCount();

    initCustomTheme();

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);

    SwitchSys::init();

    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 180.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_left", 15.0f);
    brls::getStyle().addMetric("brls/sidebar/padding_right", 15.0f);
    brls::getStyle().addMetric("brls/sidebar/item_font_size", 18.0f);

    brls::Logger::info("Borealis initialized successfully");

    curl_global_init(CURL_GLOBAL_ALL);
    brls::Logger::info("CURL initialized");

    SettingsManager::getInstance();
    brls::Logger::info("Settings loaded");

    brls::Application::registerXMLView("HomeTab", HomeTab::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);
    brls::Application::registerXMLView("LibraryTab", LibraryTab::create);
    brls::Application::registerXMLView("CollectionsTab", CollectionsTab::create);
    brls::Application::registerXMLView("PlaylistsTab", PlaylistsTab::create);
    brls::Application::registerXMLView("ServerListTab", ServerListTab::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);
    brls::Application::registerXMLView("ConfigViewTab", ConfigViewTab::create);

    brls::Application::createWindow("Saffron");
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::getPlatform()->exitToHomeMode(true);
    brls::Logger::info("Window created");

    brls::Application::pushActivity(new MainActivity());
    brls::Logger::info("Activity pushed");

    brls::Application::getExitEvent()->subscribe([]() {
        ImageLoader::cancelAll();
    });

    while (brls::Application::mainLoop()) {
    }

    brls::Logger::info("Application exiting");
    SwitchSys::exit();
    MPVCore::destroyInstance();
    brls::ThreadPool::shutdown();
    brls::Threading::stop();
    ImageQueue::shutdown();
    CurlPool::shutdown();
    curl_global_cleanup();
    return EXIT_SUCCESS;
}
