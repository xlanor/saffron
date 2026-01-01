#include "views/library_browse_view.hpp"
#include "views/media_detail_view.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"

LibraryBrowseView::LibraryBrowseView(PlexServer* server, const plex::Library& library)
    : m_server(server), m_library(library) {

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setPadding(30);

    auto* header = new brls::Box();
    header->setAxis(brls::Axis::ROW);
    header->setWidthPercentage(100);
    header->setHeight(brls::View::AUTO);
    header->setMarginBottom(20);
    header->setAlignItems(brls::AlignItems::CENTER);
    this->addView(header);

    m_titleLabel = new brls::Label();
    m_titleLabel->setText(library.title);
    m_titleLabel->setFontSize(24);
    m_titleLabel->setGrow(1);
    header->addView(m_titleLabel);

    m_loadingBox = new brls::Box();
    m_loadingBox->setWidthPercentage(100);
    m_loadingBox->setHeight(100);
    m_loadingBox->setJustifyContent(brls::JustifyContent::CENTER);
    m_loadingBox->setAlignItems(brls::AlignItems::CENTER);

    auto* loadingLabel = new brls::Label();
    loadingLabel->setText("Loading...");
    loadingLabel->setFontSize(18);
    loadingLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_loadingBox->addView(loadingLabel);
    this->addView(m_loadingBox);

    m_scrollView = new brls::ScrollingFrame();
    m_scrollView->setWidthPercentage(100);
    m_scrollView->setGrow(1);
    m_scrollView->setVisibility(brls::Visibility::GONE);
    this->addView(m_scrollView);

    m_gridView = new MediaGridView();
    m_gridView->setServer(server);
    m_gridView->setOnItemClick([this](const plex::MediaItem& item) {
        auto* detailView = new MediaDetailView(m_server, item);
        brls::Application::pushActivity(new brls::Activity(detailView));
    });
    m_scrollView->setContentView(m_gridView);
}

LibraryBrowseView::~LibraryBrowseView() {}

void LibraryBrowseView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    loadItems();
}

static constexpr int PAGE_SIZE = 30;

void LibraryBrowseView::loadItems() {
    brls::Logger::info("Loading items from library: {}", m_library.title);

    // Set up load-more callback
    m_gridView->setOnLoadMore([this]() {
        loadMoreItems();
    });

    PlexApi::getLibraryItems(
        m_server,
        m_library.key,
        0,
        PAGE_SIZE,
        [this](std::vector<plex::MediaItem> items, int totalSize) {
            brls::Logger::info("Loaded {} items (total: {})", items.size(), totalSize);

            m_loadingBox->setVisibility(brls::Visibility::GONE);
            m_scrollView->setVisibility(brls::Visibility::VISIBLE);

            m_gridView->setTotalItems(totalSize);
            m_gridView->setItems(items);
        },
        [this](const std::string& error) {
            brls::Logger::error("Failed to load items: {}", error);
            brls::Application::notify("Failed to load items");
        }
    );
}

void LibraryBrowseView::loadMoreItems() {
    int currentOffset = m_gridView->getLoadedCount();
    brls::Logger::debug("LibraryBrowseView: Loading more items from offset {}", currentOffset);

    PlexApi::getLibraryItems(
        m_server,
        m_library.key,
        currentOffset,
        PAGE_SIZE,
        [this](std::vector<plex::MediaItem> items, int) {
            brls::Logger::debug("LibraryBrowseView: Loaded {} more items", items.size());
            m_gridView->appendItems(items);
        },
        [](const std::string& error) {
            brls::Logger::error("LibraryBrowseView: Failed to load more items: {}", error);
        }
    );
}
