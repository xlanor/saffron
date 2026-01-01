#include "views/library_tab.hpp"
#include "views/media_grid_view.hpp"
#include "views/media_detail_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"

LibraryTab* LibraryTab::s_currentInstance = nullptr;
bool LibraryTab::s_isActive = false;

LibraryTab::LibraryTab() {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/library.xml");
    m_settings = SettingsManager::getInstance();

    m_gridView = new MediaGridView();
    if (contentContainer) {
        auto* scrollFrame = new brls::ScrollingFrame();
        scrollFrame->setWidthPercentage(100);
        scrollFrame->setGrow(1.0f);
        scrollFrame->setContentView(m_gridView);
        contentContainer->addView(scrollFrame);
    }

    m_gridView->setOnItemClick([this](const plex::MediaItem& item) {
        if (m_server) {
            auto* detailView = new MediaDetailView(m_server, item);
            brls::Application::pushActivity(new brls::Activity(detailView));
        }
    });
}

LibraryTab::~LibraryTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
}

brls::View* LibraryTab::create() {
    return new LibraryTab();
}

void LibraryTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;
    loadLibraries();
}

void LibraryTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;
}

void LibraryTab::loadLibraries() {
    m_server = m_settings->getCurrentServer();

    if (!m_server) {
        if (serverLabel) {
            serverLabel->setText("No server selected");
        }
        if (librarySelector) {
            librarySelector->setVisibility(brls::Visibility::GONE);
        }
        if (emptyMessage) {
            emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        }
        if (contentContainer) {
            contentContainer->setVisibility(brls::Visibility::GONE);
        }
        return;
    }

    if (serverLabel) {
        serverLabel->setText(m_server->getName());
    }

    brls::Logger::debug("LibraryTab: Loading libraries from {}", m_server->getName());

    PlexApi::getLibrarySections(
        m_server,
        [this](std::vector<plex::Library> libraries) {
            if (!s_isActive || s_currentInstance != this) return;

            m_libraries.clear();
            std::vector<std::string> libraryNames;

            for (const auto& library : libraries) {
                brls::Logger::info("LibraryTab: Found library '{}' type '{}'", library.title, library.type);
                if (library.type != "movie" && library.type != "show") {
                    continue;
                }
                m_libraries.push_back(library);
                libraryNames.push_back(library.title);
            }

            if (m_libraries.empty()) {
                if (librarySelector) {
                    librarySelector->setVisibility(brls::Visibility::GONE);
                }
                if (emptyMessage) {
                    emptyMessage->setVisibility(brls::Visibility::VISIBLE);
                }
                if (contentContainer) {
                    contentContainer->setVisibility(brls::Visibility::GONE);
                }
                return;
            }

            if (librarySelector) {
                librarySelector->setVisibility(brls::Visibility::VISIBLE);
                librarySelector->init(
                    "Library",
                    libraryNames,
                    0,
                    [this](int index) {
                        onLibrarySelected(index);
                    }
                );
            }

            if (emptyMessage) {
                emptyMessage->setVisibility(brls::Visibility::GONE);
            }
            if (contentContainer) {
                contentContainer->setVisibility(brls::Visibility::VISIBLE);
            }

            onLibrarySelected(0);

            brls::Logger::debug("LibraryTab: Loaded {} libraries", m_libraries.size());
        },
        [this](const std::string& error) {
            brls::Logger::error("LibraryTab: Failed to load libraries: {}", error);
            brls::Application::notify("Failed to load libraries");
        }
    );
}

void LibraryTab::onLibrarySelected(int index) {
    brls::Logger::info("LibraryTab: onLibrarySelected called with index {}", index);

    if (index < 0 || index >= static_cast<int>(m_libraries.size())) {
        brls::Logger::error("LibraryTab: Invalid library index {}", index);
        return;
    }

    m_selectedLibraryIndex = index;
    brls::Logger::info("LibraryTab: Selecting library '{}'", m_libraries[index].title);
    loadLibraryContent(m_libraries[index]);
}

void LibraryTab::loadLibraryContent(const plex::Library& library) {
    if (!m_server || !m_gridView) {
        return;
    }

    brls::Logger::debug("LibraryTab: Loading content from library: {}", library.title);

    m_gridView->setServer(m_server);
    m_gridView->clearItems();

    // Set up load-more callback
    m_gridView->setOnLoadMore([this]() {
        loadMoreItems();
    });

    // Load first page
    PlexApi::getLibraryItems(
        m_server,
        library.key,
        0,
        PAGE_SIZE,
        [this](std::vector<plex::MediaItem> items, int totalSize) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::debug("LibraryTab: Loaded {} items (total: {})", items.size(), totalSize);
            m_gridView->setTotalItems(totalSize);
            m_gridView->setItems(items);
        },
        [](const std::string& error) {
            brls::Logger::error("LibraryTab: Failed to load items: {}", error);
        }
    );
}

void LibraryTab::loadMoreItems() {
    if (!m_server || !m_gridView || m_selectedLibraryIndex < 0) {
        return;
    }

    int currentOffset = m_gridView->getLoadedCount();
    const auto& library = m_libraries[m_selectedLibraryIndex];

    brls::Logger::debug("LibraryTab: Loading more items from offset {}", currentOffset);

    PlexApi::getLibraryItems(
        m_server,
        library.key,
        currentOffset,
        PAGE_SIZE,
        [this](std::vector<plex::MediaItem> items, int totalSize) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::debug("LibraryTab: Loaded {} more items", items.size());
            m_gridView->appendItems(items);
        },
        [](const std::string& error) {
            brls::Logger::error("LibraryTab: Failed to load more items: {}", error);
        }
    );
}
