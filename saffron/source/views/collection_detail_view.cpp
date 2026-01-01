#include "views/collection_detail_view.hpp"
#include "views/media_detail_view.hpp"
#include "views/show_detail_view.hpp"
#include "views/media_grid_view.hpp"
#include "view/recycling_grid.hpp"
#include "view/media_card_data_source.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

CollectionDetailView* CollectionDetailView::s_instance = nullptr;

CollectionDetailView::CollectionDetailView(PlexServer* server, const plex::Collection& collection)
    : m_server(server), m_collection(collection) {

    s_instance = this;

    this->setAxis(brls::Axis::ROW);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setPadding(40);

    this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    setupUI();
    updateUI();
}

CollectionDetailView::~CollectionDetailView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void CollectionDetailView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    m_isVisible = true;
    if (!m_itemsLoaded) {
        loadCollectionItems();
    }
}

void CollectionDetailView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    m_isVisible = false;
    ImageLoader::cancel(m_posterImage);
    if (m_grid) {
        m_grid->cancelAllPendingImages();
    }
}

brls::View* CollectionDetailView::getDefaultFocus() {
    return m_grid;
}

void CollectionDetailView::setupUI() {
    auto* leftPanel = new brls::Box();
    leftPanel->setAxis(brls::Axis::COLUMN);
    leftPanel->setWidth(280);
    leftPanel->setHeightPercentage(100);
    leftPanel->setMarginRight(30);
    this->addView(leftPanel);

    m_posterImage = new brls::Image();
    m_posterImage->setWidth(280);
    m_posterImage->setHeight(420);
    m_posterImage->setScalingType(brls::ImageScalingType::FILL);
    m_posterImage->setCornerRadius(8);
    m_posterImage->setMarginBottom(20);
    leftPanel->addView(m_posterImage);

    m_titleLabel = new brls::Label();
    m_titleLabel->setFontSize(24);
    m_titleLabel->setMarginBottom(8);
    leftPanel->addView(m_titleLabel);

    m_countLabel = new brls::Label();
    m_countLabel->setFontSize(16);
    m_countLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_countLabel->setMarginBottom(15);
    leftPanel->addView(m_countLabel);

    auto* summaryScroll = new brls::ScrollingFrame();
    summaryScroll->setWidthPercentage(100);
    summaryScroll->setGrow(1);
    leftPanel->addView(summaryScroll);

    m_summaryLabel = new brls::Label();
    m_summaryLabel->setFontSize(14);
    m_summaryLabel->setTextColor(nvgRGBA(180, 180, 180, 255));
    summaryScroll->setContentView(m_summaryLabel);

    auto* rightPanel = new brls::Box();
    rightPanel->setAxis(brls::Axis::COLUMN);
    rightPanel->setGrow(1);
    rightPanel->setHeightPercentage(100);
    this->addView(rightPanel);

    m_grid = new RecyclingGrid();
    m_grid->setGrow(1);
    m_grid->spanCount = 4;
    m_grid->estimatedRowHeight = 340;
    m_grid->estimatedRowSpace = 15;
    m_grid->registerCell("MediaCard", MediaCardView::create);
    rightPanel->addView(m_grid);

    m_dataSource = new MediaCardDataSource(m_server);
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

void CollectionDetailView::updateUI() {
    m_titleLabel->setText(m_collection.title);
    m_countLabel->setText(std::to_string(m_collection.childCount) + " items");

    if (!m_collection.summary.empty()) {
        m_summaryLabel->setText(m_collection.summary);
    }

    if (!m_collection.thumb.empty() && m_server) {
        std::string posterUrl = m_server->getTranscodePictureUrl(m_collection.thumb, 280, 420);
        ImageLoader::load(m_posterImage, posterUrl);
    }
}

void CollectionDetailView::loadCollectionItems() {
    if (!m_server || !m_grid || !m_dataSource) return;

    m_grid->showSkeleton();
    int requestId = ++m_requestId;
    brls::Application::blockInputs();

    PlexApi::getCollectionItems(
        m_server,
        m_collection.ratingKey,
        [this, requestId](std::vector<plex::MediaItem> items) {
            brls::Application::unblockInputs();
            if (!s_instance) return;
            if (s_instance->m_requestId != requestId) return;
            if (!s_instance->m_isVisible) return;

            s_instance->m_itemsLoaded = true;
            s_instance->m_dataSource->setData(items);
            s_instance->m_grid->setDataSource(s_instance->m_dataSource);
            brls::Application::giveFocus(s_instance->m_grid);
        },
        [requestId](const std::string& error) {
            brls::Application::unblockInputs();
            brls::Logger::error("Failed to load collection items: {}", error);
            if (s_instance && s_instance->m_requestId == requestId && s_instance->m_isVisible) {
                s_instance->m_grid->setError(error);
            }
        }
    );
}
