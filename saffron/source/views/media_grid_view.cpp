#include "views/media_grid_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "util/image_loader.hpp"

static const float CARD_SPACING = 8.0f;
static const float CARD_WIDTH = 200.0f;
static const float POSTER_HEIGHT = 280.0f;
static const float TEXT_AREA_HEIGHT = 55.0f;

MediaGridView::MediaGridView() {
    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeight(brls::View::AUTO);

    m_gridContainer = new brls::Box();
    m_gridContainer->setAxis(brls::Axis::COLUMN);
    m_gridContainer->setWidthPercentage(100);
    m_gridContainer->setHeight(brls::View::AUTO);
    this->addView(m_gridContainer);
}

MediaGridView::~MediaGridView() {
    cancelPendingBatches();
    m_cards.clear();
}

void MediaGridView::cancelPendingBatches() {
    if (m_batchTimerId > 0) {
        brls::cancelDelay(m_batchTimerId);
        m_batchTimerId = 0;
    }
    m_pendingItems.clear();
}

void MediaGridView::processBatch() {
    m_batchTimerId = 0;

    int processed = 0;
    while (!m_pendingItems.empty() && processed < ITEMS_PER_BATCH) {
        addItemToGrid(m_pendingItems.front());
        m_pendingItems.erase(m_pendingItems.begin());
        processed++;
    }

    if (!m_pendingItems.empty()) {
        m_batchTimerId = brls::delay(16, [this]() {
            processBatch();
        });
    } else {
        m_isLoading = false;
    }
}

void MediaGridView::setServer(PlexServer* server) {
    m_server = server;
}

void MediaGridView::setItems(const std::vector<plex::MediaItem>& items) {
    clearItems();
    appendItems(items);
}

void MediaGridView::appendItems(const std::vector<plex::MediaItem>& items) {
    m_pendingItems.insert(m_pendingItems.end(), items.begin(), items.end());

    if (m_batchTimerId == 0 && !m_pendingItems.empty()) {
        m_batchTimerId = brls::delay(0, [this]() {
            processBatch();
        });
    }
}

void MediaGridView::setOnLoadMore(std::function<void()> callback) {
    m_onLoadMore = callback;
}

void MediaGridView::checkLoadMore() {
    if (m_isLoading || !m_onLoadMore || !hasMoreItems()) {
        return;
    }
    m_isLoading = true;
    m_onLoadMore();
}

void MediaGridView::clearItems() {
    cancelPendingBatches();
    if (m_gridContainer) {
        m_gridContainer->clearViews();
    }
    m_cards.clear();
    m_currentRow = nullptr;
    m_totalItems = 0;
    m_isLoading = false;
}

void MediaGridView::setOnItemClick(std::function<void(const plex::MediaItem&)> callback) {
    m_onItemClick = callback;
}

void MediaGridView::createNewRow() {
    m_currentRow = new brls::Box();
    m_currentRow->setAxis(brls::Axis::ROW);
    m_currentRow->setWidthPercentage(100);
    m_currentRow->setHeight(brls::View::AUTO);
    m_currentRow->setAlignItems(brls::AlignItems::FLEX_START);
    m_currentRow->setMarginBottom(CARD_SPACING);
    m_gridContainer->addView(m_currentRow);
}

void MediaGridView::addItemToGrid(const plex::MediaItem& item) {
    if (!m_currentRow || m_currentRow->getChildren().size() >= static_cast<size_t>(m_itemsPerRow)) {
        createNewRow();
    }

    auto* card = new MediaCardView();
    card->setData(m_server, item);
    card->setOnClick([this, item]() {
        if (m_onItemClick) {
            m_onItemClick(item);
        }
    });

    // When card gets focus, check if we need to load more
    int cardIndex = static_cast<int>(m_cards.size());
    card->getFocusEvent()->subscribe([this, cardIndex](brls::View*) {
        // Load more when focus is within last 2 rows worth of items
        int threshold = getLoadedCount() - (m_itemsPerRow * 2);
        if (cardIndex >= threshold) {
            checkLoadMore();
        }
    });

    m_currentRow->addView(card);
    m_cards.push_back(card);
}

MediaCardView::MediaCardView() {
    this->setAxis(brls::Axis::COLUMN);
    this->setWidth(CARD_WIDTH);
    this->setHeight(POSTER_HEIGHT + TEXT_AREA_HEIGHT);
    this->setMinHeight(POSTER_HEIGHT + TEXT_AREA_HEIGHT);
    this->setMaxHeight(POSTER_HEIGHT + TEXT_AREA_HEIGHT);
    this->setGrow(0);
    this->setShrink(0);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(8);

    m_posterImage = new brls::Image();
    m_posterImage->setWidthPercentage(100);
    m_posterImage->setHeight(POSTER_HEIGHT);
    m_posterImage->setScalingType(brls::ImageScalingType::FILL);
    m_posterImage->setCornerRadius(8);
    this->addView(m_posterImage);

    m_textContainer = new brls::Box();
    m_textContainer->setAxis(brls::Axis::COLUMN);
    m_textContainer->setPaddingTop(8);
    m_textContainer->setPaddingLeft(8);
    m_textContainer->setPaddingRight(8);
    m_textContainer->setWidthPercentage(100);
    m_textContainer->setHeight(TEXT_AREA_HEIGHT);
    this->addView(m_textContainer);

    m_titleLabel = new brls::Label();
    m_titleLabel->setFontSize(16);
    m_titleLabel->setWidthPercentage(100);
    m_titleLabel->setSingleLine(true);
    m_titleLabel->setMarginBottom(4);
    m_textContainer->addView(m_titleLabel);
    registerFocusableLabel(m_titleLabel);

    m_subtitleLabel = new brls::Label();
    m_subtitleLabel->setFontSize(14);
    m_subtitleLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_subtitleLabel->setWidthPercentage(100);
    m_subtitleLabel->setSingleLine(true);
    m_textContainer->addView(m_subtitleLabel);
    registerFocusableLabel(m_subtitleLabel, true);

    m_progressBar = new brls::Box();
    m_progressBar->setWidthPercentage(100);
    m_progressBar->setHeight(3);
    m_progressBar->setBackgroundColor(nvgRGBA(80, 80, 80, 255));
    m_progressBar->setMarginTop(4);
    m_progressBar->setVisibility(brls::Visibility::GONE);
    m_textContainer->addView(m_progressBar);
}

RecyclingGridItem* MediaCardView::create() {
    return new MediaCardView();
}

void MediaCardView::prepareForReuse() {
    m_posterImage->clear();
    m_titleLabel->setText("");
    m_subtitleLabel->setText("");
    m_progressBar->setVisibility(brls::Visibility::GONE);
    m_progressBar->clearViews();
    m_progressFill = nullptr;
    registerProgressBar(nullptr, nullptr);
}

void MediaCardView::cacheForReuse() {
    ImageLoader::cancel(m_posterImage);
    m_posterImage->clear();
}

void MediaCardView::cancelPendingRequests() {
    ImageLoader::cancel(m_posterImage);
}

void MediaCardView::setData(PlexServer* server, const plex::MediaItem& item) {
    m_server = server;
    m_item = item;

    std::string displayTitle = item.title;
    if (!item.editionTitle.empty()) {
        displayTitle += " (" + item.editionTitle + ")";
    }
    m_titleLabel->setText(displayTitle);

    if (item.mediaType == plex::MediaType::Movie) {
        std::string subtitle = std::to_string(item.year);
        if (!item.media.empty()) {
            bool has4k = false, has1080 = false, has720 = false;
            for (const auto& m : item.media) {
                std::string res = m.videoResolution;
                if (res == "4k" || res == "4K") has4k = true;
                else if (res == "1080" || res == "1080p") has1080 = true;
                else if (res == "720" || res == "720p") has720 = true;
            }
            if (has720) subtitle += " • 720p";
            if (has1080) subtitle += " • 1080p";
            if (has4k) subtitle += " • 4K";
        }
        m_subtitleLabel->setText(subtitle);
    } else if (item.mediaType == plex::MediaType::Show) {
        m_subtitleLabel->setText("TV Show");
    } else if (item.mediaType == plex::MediaType::Episode) {
        m_subtitleLabel->setText("S" + std::to_string(item.parentIndex) +
                                 " E" + std::to_string(item.index));
    } else {
        m_subtitleLabel->setText("");
    }

    if (item.viewOffset > 0 && item.duration > 0) {
        m_progressBar->setVisibility(brls::Visibility::VISIBLE);
        m_progressBar->clearViews();

        float progress = static_cast<float>(item.viewOffset) / static_cast<float>(item.duration);
        m_progressFill = new brls::Box();
        m_progressFill->setWidthPercentage(progress * 100.0f);
        m_progressFill->setHeight(3);
        m_progressFill->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
        m_progressBar->addView(m_progressFill);
        registerProgressBar(m_progressBar, m_progressFill);
    } else {
        m_progressFill = nullptr;
        registerProgressBar(nullptr, nullptr);
    }

    loadPosterImage();
}

void MediaCardView::setOnClick(std::function<void()> callback) {
    this->registerClickAction([callback](brls::View*) {
        if (callback) callback();
        return true;
    });
}

std::string MediaCardView::buildImageUrl() {
    if (!m_server || m_item.thumb.empty()) {
        return "";
    }
    return m_server->getTranscodePictureUrl(m_item.thumb, 200, 300);
}

void MediaCardView::loadPosterImage() {
    std::string url = buildImageUrl();
    if (url.empty()) {
        return;
    }

    ImageLoader::load(m_posterImage, url);
}
