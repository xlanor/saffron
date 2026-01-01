#include "views/tag_media_view.hpp"
#include "views/media_detail_view.hpp"
#include "views/show_detail_view.hpp"
#include "views/media_grid_view.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

static const int PAGE_SIZE = 50;

TagMediaView* TagMediaView::s_instance = nullptr;

TagMediaView::TagMediaView(PlexServer* server, TagType type, int tagId,
                           const std::string& tagName, const std::string& tagThumb,
                           int librarySectionId)
    : m_server(server), m_tagType(type), m_tagId(tagId),
      m_tagName(tagName), m_tagThumb(tagThumb), m_librarySectionId(librarySectionId) {

    s_instance = this;

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    setupUI();
    loadMedia();
}

TagMediaView::~TagMediaView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    if (m_recyclerGrid) {
        m_recyclerGrid->clearData();
    }
}

brls::View* TagMediaView::getDefaultFocus() {
    if (m_dataLoaded && m_recyclerGrid) {
        return m_recyclerGrid;
    }
    if (m_focusPlaceholder) {
        return m_focusPlaceholder;
    }
    return this;
}

void TagMediaView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    brls::View* focus = getDefaultFocus();
    if (focus) {
        brls::Application::giveFocus(focus);
    }
}

void TagMediaView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    ImageQueue::instance().clear();
    if (m_recyclerGrid) {
        m_recyclerGrid->cancelAllPendingImages();
    }
}

std::string TagMediaView::getTagTypeLabel() const {
    switch (m_tagType) {
        case TagType::Actor: return "Actor";
        case TagType::Genre: return "Genre";
        case TagType::Director: return "Director";
        case TagType::Writer: return "Writer";
        case TagType::Collection: return "Collection";
        default: return "";
    }
}

std::string TagMediaView::buildApiEndpoint() const {
    if (m_tagType == TagType::Actor) {
        return "/library/people/" + std::to_string(m_tagId) + "/media";
    }

    std::string filter;
    switch (m_tagType) {
        case TagType::Genre: filter = "genre"; break;
        case TagType::Director: filter = "director"; break;
        case TagType::Writer: filter = "writer"; break;
        case TagType::Collection: filter = "collection"; break;
        default: filter = "genre"; break;
    }

    return "/library/sections/" + std::to_string(m_librarySectionId) +
           "/all?" + filter + "=" + std::to_string(m_tagId);
}

void TagMediaView::setupUI() {
    auto* headerBox = new brls::Box();
    headerBox->setAxis(brls::Axis::ROW);
    headerBox->setWidthPercentage(100);
    headerBox->setHeight(160);
    headerBox->setPadding(40);
    headerBox->setAlignItems(brls::AlignItems::CENTER);
    headerBox->setBackgroundColor(nvgRGBA(30, 30, 30, 255));
    this->addView(headerBox);

    m_tagImageContainer = new brls::Box();
    m_tagImageContainer->setWidth(100);
    m_tagImageContainer->setHeight(100);
    m_tagImageContainer->setMarginRight(30);
    m_tagImageContainer->setJustifyContent(brls::JustifyContent::CENTER);
    m_tagImageContainer->setAlignItems(brls::AlignItems::CENTER);
    headerBox->addView(m_tagImageContainer);

    if (!m_tagThumb.empty() && m_server) {
        m_tagImage = new brls::Image();
        m_tagImage->setWidth(100);
        m_tagImage->setHeight(100);
        m_tagImage->setScalingType(brls::ImageScalingType::FILL);
        m_tagImage->setCornerRadius(m_tagType == TagType::Actor ? 50 : 8);
        m_tagImageContainer->addView(m_tagImage);

        std::string thumbUrl = m_server->getTranscodePictureUrl(m_tagThumb, 100, 100);
        ImageLoader::load(m_tagImage, thumbUrl);
    } else {
        auto* iconLabel = new brls::Label();
        iconLabel->setFontSize(40);
        iconLabel->setTextColor(nvgRGBA(229, 160, 13, 255));
        switch (m_tagType) {
            case TagType::Actor: iconLabel->setText("person"); break;
            case TagType::Genre: iconLabel->setText("category"); break;
            case TagType::Director: iconLabel->setText("movie"); break;
            case TagType::Writer: iconLabel->setText("edit"); break;
            case TagType::Collection: iconLabel->setText("folder"); break;
        }
        m_tagImageContainer->addView(iconLabel);
    }

    auto* infoBox = new brls::Box();
    infoBox->setAxis(brls::Axis::COLUMN);
    infoBox->setGrow(1);
    headerBox->addView(infoBox);

    m_nameLabel = new brls::Label();
    m_nameLabel->setFontSize(28);
    m_nameLabel->setText(m_tagName);
    m_nameLabel->setMarginBottom(8);
    infoBox->addView(m_nameLabel);

    m_typeLabel = new brls::Label();
    m_typeLabel->setFontSize(16);
    m_typeLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_typeLabel->setText(getTagTypeLabel());
    infoBox->addView(m_typeLabel);

    m_countLabel = new brls::Label();
    m_countLabel->setFontSize(14);
    m_countLabel->setTextColor(nvgRGBA(120, 120, 120, 255));
    m_countLabel->setVisibility(brls::Visibility::GONE);
    m_countLabel->setMarginTop(4);
    infoBox->addView(m_countLabel);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::COLUMN);
    contentBox->setWidthPercentage(100);
    contentBox->setGrow(1);
    contentBox->setPaddingTop(20);
    contentBox->setPaddingLeft(40);
    contentBox->setPaddingRight(40);
    this->addView(contentBox);

    m_recyclerGrid = new RecyclingGrid();
    m_recyclerGrid->setWidthPercentage(100);
    m_recyclerGrid->setGrow(1);
    m_recyclerGrid->spanCount = 5;
    m_recyclerGrid->estimatedRowHeight = 335;
    m_recyclerGrid->estimatedRowSpace = 15;
    m_recyclerGrid->registerCell("cell", []() { return MediaCardView::create(); });
    contentBox->addView(m_recyclerGrid);

    auto* dataSource = new TagMediaDataSource(m_server, [this](const plex::MediaItem& item) {
        if (item.mediaType == plex::MediaType::Show) {
            auto* showView = new ShowDetailView(m_server, item);
            brls::Application::pushActivity(new brls::Activity(showView));
        } else {
            auto* detailView = new MediaDetailView(m_server, item);
            brls::Application::pushActivity(new brls::Activity(detailView));
        }
    });
    m_recyclerGrid->setDataSource(dataSource);

    m_recyclerGrid->onNextPage([this]() {
        if (!m_isLoading && m_loadedItems < m_totalItems) {
            loadMedia();
        }
    });

    m_spinnerContainer = new brls::Box();
    m_spinnerContainer->setWidthPercentage(100);
    m_spinnerContainer->setHeight(200);
    m_spinnerContainer->setJustifyContent(brls::JustifyContent::CENTER);
    m_spinnerContainer->setAlignItems(brls::AlignItems::CENTER);
    m_spinnerContainer->setPositionType(brls::PositionType::ABSOLUTE);
    m_spinnerContainer->setPositionTop(200);
    m_spinnerContainer->setPositionLeft(0);
    m_spinnerContainer->setPositionRight(0);
    m_spinnerContainer->addView(new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE));
    this->addView(m_spinnerContainer);

    m_focusPlaceholder = new brls::Box();
    m_focusPlaceholder->setWidthPercentage(100);
    m_focusPlaceholder->setHeight(1);
    m_focusPlaceholder->setFocusable(true);
    m_focusPlaceholder->setHideHighlightBackground(true);
    m_focusPlaceholder->setHideHighlightBorder(true);
    this->addView(m_focusPlaceholder);

    m_emptyLabel = new brls::Label();
    m_emptyLabel->setFontSize(16);
    m_emptyLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_emptyLabel->setText("No items found");
    m_emptyLabel->setVisibility(brls::Visibility::GONE);
    m_emptyLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    contentBox->addView(m_emptyLabel);
}

void TagMediaView::loadMedia() {
    if (!m_server || m_isLoading) return;

    m_isLoading = true;
    int requestId = ++m_requestId;

    PlexApi::getTagMedia(
        m_server,
        buildApiEndpoint(),
        m_loadedItems,
        PAGE_SIZE,
        [this, requestId](std::vector<plex::MediaItem> items, int totalSize) {
            if (!s_instance || requestId != m_requestId) return;

            m_spinnerContainer->setVisibility(brls::Visibility::GONE);
            m_isLoading = false;

            if (m_totalItems == 0) {
                m_totalItems = totalSize;
                m_countLabel->setText(std::to_string(totalSize) + " items");
                m_countLabel->setVisibility(brls::Visibility::VISIBLE);
            }

            if (items.empty() && m_loadedItems == 0) {
                m_emptyLabel->setVisibility(brls::Visibility::VISIBLE);
                return;
            }

            appendMedia(items);

            if (!m_dataLoaded) {
                m_dataLoaded = true;
                if (m_focusPlaceholder) {
                    m_focusPlaceholder->setVisibility(brls::Visibility::GONE);
                }
                if (m_recyclerGrid) {
                    brls::Application::giveFocus(m_recyclerGrid);
                }
            }
        },
        [this, requestId](const std::string& error) {
            if (!s_instance || requestId != m_requestId) return;

            brls::Logger::error("Failed to load tag media: {}", error);
            m_spinnerContainer->setVisibility(brls::Visibility::GONE);
            m_isLoading = false;

            if (m_loadedItems == 0) {
                m_emptyLabel->setText("Failed to load items");
                m_emptyLabel->setVisibility(brls::Visibility::VISIBLE);
            }
        }
    );
}

void TagMediaView::appendMedia(const std::vector<plex::MediaItem>& items) {
    auto* dataSource = dynamic_cast<TagMediaDataSource*>(m_recyclerGrid->getDataSource());
    if (dataSource) {
        dataSource->appendItems(items);
        m_loadedItems += items.size();
        m_recyclerGrid->notifyDataChanged();
    }
}

TagMediaDataSource::TagMediaDataSource(PlexServer* server,
                                       std::function<void(const plex::MediaItem&)> onItemClick)
    : m_server(server), m_onItemClick(onItemClick) {}

size_t TagMediaDataSource::getItemCount() {
    return m_items.size();
}

RecyclingGridItem* TagMediaDataSource::cellForRow(RecyclingView* recycler, size_t index) {
    auto* cell = dynamic_cast<MediaCardView*>(recycler->dequeueReusableCell("cell"));
    if (!cell) {
        cell = new MediaCardView();
    }

    if (index < m_items.size()) {
        cell->setData(m_server, m_items[index]);
        plex::MediaItem item = m_items[index];
        cell->setOnClick([this, item]() {
            if (m_onItemClick) {
                m_onItemClick(item);
            }
        });
    }

    return cell;
}

void TagMediaDataSource::onItemSelected(brls::Box* recycler, size_t index) {
    if (index < m_items.size() && m_onItemClick) {
        m_onItemClick(m_items[index]);
    }
}

void TagMediaDataSource::clearData() {
    m_items.clear();
}

void TagMediaDataSource::appendItems(const std::vector<plex::MediaItem>& items) {
    m_items.insert(m_items.end(), items.begin(), items.end());
}
