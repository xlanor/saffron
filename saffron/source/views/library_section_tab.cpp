#include "views/library_section_tab.hpp"
#include "views/media_grid_view.hpp"
#include "views/media_detail_view.hpp"
#include "views/show_detail_view.hpp"
#include "view/recycling_grid.hpp"
#include "view/media_card_data_source.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"

LibrarySectionTab* LibrarySectionTab::s_currentInstance = nullptr;
bool LibrarySectionTab::s_isActive = false;
std::unordered_map<int, LibraryCacheEntry> LibrarySectionTab::s_cache;

LibrarySectionTab::LibrarySectionTab(PlexServer* server, const plex::Library& library)
    : m_server(server), m_library(library) {

    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20);

    m_grid = new RecyclingGrid();
    m_grid->setGrow(1.0f);
    m_grid->spanCount = 4;
    m_grid->estimatedRowHeight = 340;
    m_grid->estimatedRowSpace = 15;
    m_grid->registerCell("MediaCard", MediaCardView::create);
    this->addView(m_grid);

    m_spinnerContainer = new brls::Box();
    m_spinnerContainer->setWidthPercentage(100);
    m_spinnerContainer->setHeightPercentage(100);
    m_spinnerContainer->setPositionType(brls::PositionType::ABSOLUTE);
    m_spinnerContainer->setPositionTop(0);
    m_spinnerContainer->setJustifyContent(brls::JustifyContent::CENTER);
    m_spinnerContainer->setAlignItems(brls::AlignItems::CENTER);
    m_spinnerContainer->addView(new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE));
    this->addView(m_spinnerContainer);

    m_dataSource = new MediaCardDataSource(server);
    m_dataSource->setOnItemClick([this](const plex::MediaItem& item) {
        if (!m_server) return;

        if (item.mediaType == plex::MediaType::Show) {
            auto* showView = new ShowDetailView(m_server, item);
            brls::Application::pushActivity(new brls::Activity(showView));
        } else {
            auto* detailView = new MediaDetailView(m_server, item);
            brls::Application::pushActivity(new brls::Activity(detailView));
        }
    });
}

LibrarySectionTab::~LibrarySectionTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
}

void LibrarySectionTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_currentInstance = this;
    s_isActive = true;

    if (s_cache.find(m_library.key) != s_cache.end()) {
        restoreFromCache();
    } else {
        loadItems();
    }
}

void LibrarySectionTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;

    // Cancel pending image requests to prioritize new view's images
    if (m_grid) {
        m_grid->cancelAllPendingImages();
    }
}

void LibrarySectionTab::restoreFromCache() {
    if (!m_grid || !m_dataSource) return;

    auto& entry = s_cache[m_library.key];
    brls::Logger::info("LibrarySectionTab: Restoring '{}' from cache ({} items)",
                       m_library.title, entry.items.size());

    m_totalItems = entry.totalSize;
    m_dataSource->setData(entry.items);
    m_grid->setDataSource(m_dataSource);
    if (m_spinnerContainer) m_spinnerContainer->setVisibility(brls::Visibility::GONE);

    m_grid->onNextPage([this]() {
        if (static_cast<int>(m_dataSource->getItemCount()) < m_totalItems) {
            loadMoreItems();
        }
    });
}

void LibrarySectionTab::clearCache() {
    s_cache.clear();
}

void LibrarySectionTab::loadItems() {
    if (!m_server || !m_grid || !m_dataSource) {
        return;
    }

    brls::Logger::info("LibrarySectionTab: Loading '{}' (key={})", m_library.title, m_library.key);

    m_dataSource->clearData();
    m_grid->showSkeleton();
    if (m_spinnerContainer) m_spinnerContainer->setVisibility(brls::Visibility::VISIBLE);

    m_grid->onNextPage([this]() {
        if (static_cast<int>(m_dataSource->getItemCount()) < m_totalItems) {
            loadMoreItems();
        }
    });

    int libraryKey = m_library.key;

    PlexApi::getLibraryItems(
        m_server,
        m_library.key,
        0,
        PAGE_SIZE,
        [this, libraryKey](std::vector<plex::MediaItem> items, int totalSize) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::info("LibrarySectionTab: Loaded {} items (total: {})", items.size(), totalSize);
            m_totalItems = totalSize;
            m_dataSource->setData(items);
            m_grid->setDataSource(m_dataSource);
            if (m_spinnerContainer) m_spinnerContainer->setVisibility(brls::Visibility::GONE);

            s_cache[libraryKey] = {items, totalSize};
        },
        [this](const std::string& error) {
            brls::Logger::error("LibrarySectionTab: Failed to load '{}': {}", m_library.title, error);
            m_grid->setError(error);
            if (m_spinnerContainer) m_spinnerContainer->setVisibility(brls::Visibility::GONE);
        }
    );
}

void LibrarySectionTab::loadMoreItems() {
    if (!m_server || !m_grid || !m_dataSource) {
        return;
    }

    int currentOffset = static_cast<int>(m_dataSource->getItemCount());
    brls::Logger::debug("LibrarySectionTab: Loading more from offset {}", currentOffset);

    int libraryKey = m_library.key;

    PlexApi::getLibraryItems(
        m_server,
        m_library.key,
        currentOffset,
        PAGE_SIZE,
        [this, libraryKey](std::vector<plex::MediaItem> items, int) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::debug("LibrarySectionTab: Loaded {} more items", items.size());
            m_dataSource->appendData(items);
            m_grid->notifyDataChanged();

            auto& entry = s_cache[libraryKey];
            entry.items.insert(entry.items.end(), items.begin(), items.end());
        },
        [](const std::string& error) {
            brls::Logger::error("LibrarySectionTab: Failed to load more: {}", error);
        }
    );
}
