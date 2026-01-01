#include "views/collections_tab.hpp"
#include "views/collection_detail_view.hpp"
#include "views/library_browse_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

CollectionsTab* CollectionsTab::s_currentInstance = nullptr;
bool CollectionsTab::s_isActive = false;

CollectionsTab::CollectionsTab() {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/collections.xml");
    m_settings = SettingsManager::getInstance();
}

CollectionsTab::~CollectionsTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
}

brls::View* CollectionsTab::create() {
    return new CollectionsTab();
}

void CollectionsTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;
    loadCollections();
}

void CollectionsTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;

    // Cancel pending image requests to prioritize new view's images
    for (CollectionItemView* item : m_items) {
        item->cancelPendingImage();
    }
}

void CollectionsTab::loadCollections() {
    PlexServer* server = m_settings->getCurrentServer();

    if (!server) {
        if (serverLabel) {
            serverLabel->setText("No server selected");
        }
        if (emptyMessage) {
            emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        }
        if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        return;
    }

    if (serverLabel) {
        serverLabel->setText(server->getName());
    }

    if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::VISIBLE);

    PlexApi::getLibrarySections(
        server,
        [this, server](std::vector<plex::Library> libraries) {
            if (!s_isActive || s_currentInstance != this) return;

            if (collectionContainer) {
                collectionContainer->clearViews();
            }
            m_items.clear();

            for (const auto& library : libraries) {
                if (library.type != "movie" && library.type != "show") continue;

                PlexApi::getCollections(
                    server,
                    library.key,
                    [this, server](std::vector<plex::Collection> collections) {
                        if (!s_isActive || s_currentInstance != this) return;

                        for (const auto& collection : collections) {
                            auto* itemView = new CollectionItemView(server, collection);
                            if (collectionContainer) {
                                collectionContainer->addView(itemView);
                            }
                            m_items.push_back(itemView);
                        }

                        if (emptyMessage) {
                            emptyMessage->setVisibility(m_items.empty() ?
                                brls::Visibility::VISIBLE : brls::Visibility::GONE);
                        }
                        if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
                    },
                    [this](const std::string& error) {
                        brls::Logger::error("Failed to load collections: {}", error);
                        if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
                    }
                );
            }
        },
        [this](const std::string& error) {
            brls::Logger::error("Failed to load libraries: {}", error);
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        }
    );
}

CollectionItemView::CollectionItemView(PlexServer* server, const plex::Collection& collection)
    : m_server(server), m_collection(collection) {

    this->setAxis(brls::Axis::ROW);
    this->setPadding(15);
    this->setMarginBottom(10);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(8);
    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setAlignItems(brls::AlignItems::CENTER);

    m_thumbImage = new brls::Image();
    m_thumbImage->setWidth(80);
    m_thumbImage->setHeight(120);
    m_thumbImage->setScalingType(brls::ImageScalingType::FILL);
    m_thumbImage->setCornerRadius(4);
    m_thumbImage->setMarginRight(15);
    this->addView(m_thumbImage);

    auto* infoBox = new brls::Box();
    infoBox->setAxis(brls::Axis::COLUMN);
    infoBox->setGrow(1);
    this->addView(infoBox);

    m_titleLabel = new brls::Label();
    m_titleLabel->setText(collection.title);
    m_titleLabel->setFontSize(18);
    m_titleLabel->setMarginBottom(5);
    infoBox->addView(m_titleLabel);
    m_originalTitleColor = m_titleLabel->getTextColor();

    m_countLabel = new brls::Label();
    m_countLabel->setText(std::to_string(collection.childCount) + " items");
    m_countLabel->setFontSize(14);
    m_countLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    infoBox->addView(m_countLabel);
    m_originalCountColor = m_countLabel->getTextColor();

    if (!collection.thumb.empty() && server) {
        std::string url = server->getTranscodePictureUrl(collection.thumb, 80, 120);
        ImageLoader::load(m_thumbImage, url);
    }

    this->registerClickAction([this](brls::View*) {
        auto* detailView = new CollectionDetailView(m_server, m_collection);
        brls::Application::pushActivity(new brls::Activity(detailView));
        return true;
    });
}

void CollectionItemView::cancelPendingImage() {
    ImageLoader::cancel(m_thumbImage);
}

void CollectionItemView::onFocusGained() {
    Box::onFocusGained();
    this->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
    m_titleLabel->setTextColor(nvgRGB(0, 0, 0));
    m_countLabel->setTextColor(nvgRGBA(60, 60, 60, 255));
}

void CollectionItemView::onFocusLost() {
    Box::onFocusLost();
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    m_titleLabel->setTextColor(m_originalTitleColor);
    m_countLabel->setTextColor(m_originalCountColor);
}
