#include "views/home_tab.hpp"
#include "views/media_detail_view.hpp"
#include "views/season_view.hpp"
#include "views/show_detail_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

HomeTab* HomeTab::s_currentInstance = nullptr;
bool HomeTab::s_isActive = false;

static const float HUB_ITEM_WIDTH = 200.0f;
static const float HUB_ITEM_HEIGHT = 300.0f;
static const float HUB_ITEM_SPACING = 10.0f;

HomeTab::HomeTab() {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/home.xml");
    m_settings = SettingsManager::getInstance();
}

HomeTab::~HomeTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
    m_hubRows.clear();
}

brls::View* HomeTab::create() {
    return new HomeTab();
}

void HomeTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;
    loadHubs();
}

void HomeTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;
    cancelPendingImages();
}

void HomeTab::cancelPendingImages() {
    for (HubRowView* row : m_hubRows) {
        row->cancelPendingImages();
    }
}

void HomeTab::clearHubs() {
    if (hubContainer) {
        hubContainer->clearViews();
    }
    m_hubRows.clear();
}

void HomeTab::loadHubs() {
    PlexServer* server = m_settings->getCurrentServer();

    if (!server) {
        if (emptyMessage) {
            emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        }
        if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        return;
    }

    if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::VISIBLE);

    brls::Logger::info("Loading hubs from {}", server->getName());

    PlexApi::getHubs(
        server,
        [this, server](std::vector<plex::Hub> hubs) {
            if (!s_isActive || s_currentInstance != this) return;

            clearHubs();

            for (const auto& hub : hubs) {
                if (hub.items.empty()) continue;

                auto* hubRow = new HubRowView(server, hub);
                hubRow->setOnItemClick([server](const plex::MediaItem& item) {
                    if (item.mediaType == plex::MediaType::Show) {
                        auto* showView = new ShowDetailView(server, item);
                        brls::Application::pushActivity(new brls::Activity(showView));
                    } else if (item.mediaType == plex::MediaType::Season) {
                        auto* seasonView = new SeasonView(server, item);
                        brls::Application::pushActivity(new brls::Activity(seasonView));
                    } else {
                        auto* detailView = new MediaDetailView(server, item);
                        brls::Application::pushActivity(new brls::Activity(detailView));
                    }
                });

                if (hubContainer) {
                    hubContainer->addView(hubRow);
                }
                m_hubRows.push_back(hubRow);
            }

            if (emptyMessage) {
                emptyMessage->setVisibility(m_hubRows.empty() ?
                    brls::Visibility::VISIBLE : brls::Visibility::GONE);
            }
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);

            brls::Logger::info("Loaded {} hubs", m_hubRows.size());
        },
        [this](const std::string& error) {
            if (!s_isActive || s_currentInstance != this) return;

            brls::Logger::error("Failed to load hubs: {}", error);
            if (emptyMessage) {
                emptyMessage->setVisibility(brls::Visibility::VISIBLE);
            }
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        }
    );
}

HubRowView::HubRowView(PlexServer* server, const plex::Hub& hub)
    : m_server(server), m_hub(hub) {

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeight(brls::View::AUTO);
    this->setMarginBottom(30);

    m_titleLabel = new brls::Label();
    m_titleLabel->setText(hub.title);
    m_titleLabel->setFontSize(20);
    m_titleLabel->setMarginBottom(15);
    this->addView(m_titleLabel);

    m_scrollFrame = new brls::HScrollingFrame();
    m_scrollFrame->setWidthPercentage(100);
    m_scrollFrame->setHeight(HUB_ITEM_HEIGHT + 20);
    this->addView(m_scrollFrame);

    m_itemsContainer = new brls::Box();
    m_itemsContainer->setAxis(brls::Axis::ROW);
    m_itemsContainer->setWidth(brls::View::AUTO);
    m_itemsContainer->setHeight(brls::View::AUTO);
    m_scrollFrame->setContentView(m_itemsContainer);

    addItems();
}

void HubRowView::setOnItemClick(std::function<void(const plex::MediaItem&)> callback) {
    m_onItemClick = callback;
}

void HubRowView::cancelPendingImages() {
    if (!m_itemsContainer) return;
    for (brls::View* child : m_itemsContainer->getChildren()) {
        HubItemView* item = dynamic_cast<HubItemView*>(child);
        if (item) item->cancelPendingImage();
    }
}

void HubRowView::addItems() {
    for (const auto& item : m_hub.items) {
        auto* itemView = new HubItemView(m_server, item);
        itemView->setOnClick([this, item]() {
            if (m_onItemClick) {
                m_onItemClick(item);
            }
        });
        m_itemsContainer->addView(itemView);
    }
}

HubItemView::HubItemView(PlexServer* server, const plex::MediaItem& item)
    : m_server(server), m_item(item) {

    this->setAxis(brls::Axis::COLUMN);
    this->setWidth(HUB_ITEM_WIDTH);
    this->setHeight(HUB_ITEM_HEIGHT);
    this->setMarginRight(HUB_ITEM_SPACING);
    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(8);

    m_image = new brls::Image();
    m_image->setWidth(HUB_ITEM_WIDTH);
    m_image->setHeight(HUB_ITEM_HEIGHT - 60);
    m_image->setScalingType(brls::ImageScalingType::FILL);
    m_image->setCornerRadius(8);
    this->addView(m_image);

    auto* textBox = new brls::Box();
    textBox->setAxis(brls::Axis::COLUMN);
    textBox->setPadding(8);
    textBox->setWidthPercentage(100);
    textBox->setHeight(60);
    this->addView(textBox);

    m_titleLabel = new brls::Label();
    m_titleLabel->setFontSize(16);
    m_titleLabel->setSingleLine(true);
    m_titleLabel->setWidthPercentage(100);
    textBox->addView(m_titleLabel);

    m_subtitleLabel = new brls::Label();
    m_subtitleLabel->setFontSize(14);
    m_subtitleLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_subtitleLabel->setSingleLine(true);
    m_subtitleLabel->setWidthPercentage(100);
    textBox->addView(m_subtitleLabel);

    m_originalTitleColor = m_titleLabel->getTextColor();
    m_originalSubtitleColor = m_subtitleLabel->getTextColor();

    std::string displayTitle = item.title;
    if (!item.editionTitle.empty()) {
        displayTitle += " (" + item.editionTitle + ")";
    }
    m_titleLabel->setText(displayTitle);

    if (item.mediaType == plex::MediaType::Episode) {
        m_subtitleLabel->setText(item.grandparentTitle);
    } else if (item.mediaType == plex::MediaType::Movie && item.year > 0) {
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
    } else if (item.mediaType == plex::MediaType::Season) {
        m_subtitleLabel->setText(item.parentTitle);
    }

    loadImage();
}

void HubItemView::setOnClick(std::function<void()> callback) {
    this->registerClickAction([callback](brls::View*) {
        if (callback) callback();
        return true;
    });
}

void HubItemView::cancelPendingImage() {
    ImageLoader::cancel(m_image);
    m_image->clear();
}

void HubItemView::loadImage() {
    if (!m_server) return;

    std::string thumbKey = m_item.thumb;
    if (thumbKey.empty() && !m_item.grandparentThumb.empty()) {
        thumbKey = m_item.grandparentThumb;
    }
    if (thumbKey.empty() && !m_item.parentThumb.empty()) {
        thumbKey = m_item.parentThumb;
    }

    if (thumbKey.empty()) return;

    std::string url = m_server->getTranscodePictureUrl(thumbKey, 180, 270);
    ImageLoader::load(m_image, url);
}

brls::View* HubItemView::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (direction == brls::FocusDirection::LEFT) {
        brls::Box* parent = getParent();
        if (parent) {
            auto& siblings = parent->getChildren();
            if (!siblings.empty() && siblings.front() == this) {
                brls::View* p = parent;
                while (p) {
                    brls::View* next = p->getParent() ? p->getParent()->getNextFocus(direction, p) : nullptr;
                    if (next && next != p && next != this) {
                        return next;
                    }
                    p = p->getParent();
                }
            }
        }
    }
    return Box::getNextFocus(direction, currentView);
}

void HubItemView::onFocusGained() {
    Box::onFocusGained();
    this->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
    m_titleLabel->setTextColor(nvgRGB(0, 0, 0));
    m_subtitleLabel->setTextColor(nvgRGBA(60, 60, 60, 255));
}

void HubItemView::onFocusLost() {
    Box::onFocusLost();
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    m_titleLabel->setTextColor(m_originalTitleColor);
    m_subtitleLabel->setTextColor(m_originalSubtitleColor);
}
