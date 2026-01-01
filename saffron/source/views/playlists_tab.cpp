#include "views/playlists_tab.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

PlaylistsTab* PlaylistsTab::s_currentInstance = nullptr;
bool PlaylistsTab::s_isActive = false;

PlaylistsTab::PlaylistsTab() {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/playlists.xml");
    m_settings = SettingsManager::getInstance();
}

PlaylistsTab::~PlaylistsTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
}

brls::View* PlaylistsTab::create() {
    return new PlaylistsTab();
}

void PlaylistsTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;
    loadPlaylists();
}

void PlaylistsTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;

    // Cancel pending image requests to prioritize new view's images
    for (PlaylistItemView* item : m_items) {
        item->cancelPendingImage();
    }
}

void PlaylistsTab::loadPlaylists() {
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

    PlexApi::getPlaylists(
        server,
        [this, server](std::vector<plex::Playlist> playlists) {
            if (!s_isActive || s_currentInstance != this) return;

            if (playlistContainer) {
                playlistContainer->clearViews();
            }
            m_items.clear();

            for (const auto& playlist : playlists) {
                if (playlist.playlistType != "video") continue;

                auto* itemView = new PlaylistItemView(server, playlist);
                if (playlistContainer) {
                    playlistContainer->addView(itemView);
                }
                m_items.push_back(itemView);
            }

            if (emptyMessage) {
                emptyMessage->setVisibility(m_items.empty() ?
                    brls::Visibility::VISIBLE : brls::Visibility::GONE);
            }
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);

            brls::Logger::info("Loaded {} playlists", m_items.size());
        },
        [this](const std::string& error) {
            brls::Logger::error("Failed to load playlists: {}", error);
            if (emptyMessage) {
                emptyMessage->setVisibility(brls::Visibility::VISIBLE);
            }
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        }
    );
}

static std::string formatDuration(int64_t ms) {
    int64_t totalMinutes = ms / 60000;
    int hours = static_cast<int>(totalMinutes / 60);
    int minutes = static_cast<int>(totalMinutes % 60);

    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    return std::to_string(minutes) + " min";
}

PlaylistItemView::PlaylistItemView(PlexServer* server, const plex::Playlist& playlist)
    : m_server(server), m_playlist(playlist) {

    this->setAxis(brls::Axis::ROW);
    this->setPadding(15);
    this->setMarginBottom(10);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(8);
    this->setFocusable(true);
    this->setAlignItems(brls::AlignItems::CENTER);

    m_thumbImage = new brls::Image();
    m_thumbImage->setWidth(80);
    m_thumbImage->setHeight(80);
    m_thumbImage->setScalingType(brls::ImageScalingType::FILL);
    m_thumbImage->setCornerRadius(4);
    m_thumbImage->setMarginRight(15);
    this->addView(m_thumbImage);

    auto* infoBox = new brls::Box();
    infoBox->setAxis(brls::Axis::COLUMN);
    infoBox->setGrow(1);
    this->addView(infoBox);

    m_titleLabel = new brls::Label();
    m_titleLabel->setText(playlist.title);
    m_titleLabel->setFontSize(18);
    m_titleLabel->setMarginBottom(5);
    infoBox->addView(m_titleLabel);

    auto* metaRow = new brls::Box();
    metaRow->setAxis(brls::Axis::ROW);
    infoBox->addView(metaRow);

    m_countLabel = new brls::Label();
    m_countLabel->setText(std::to_string(playlist.leafCount) + " items");
    m_countLabel->setFontSize(14);
    m_countLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_countLabel->setMarginRight(15);
    metaRow->addView(m_countLabel);

    if (playlist.duration > 0) {
        m_durationLabel = new brls::Label();
        m_durationLabel->setText(formatDuration(playlist.duration));
        m_durationLabel->setFontSize(14);
        m_durationLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
        metaRow->addView(m_durationLabel);
    }

    std::string thumbKey = playlist.composite.empty() ? playlist.thumb : playlist.composite;
    if (!thumbKey.empty() && server) {
        std::string url = server->getTranscodePictureUrl(thumbKey, 80, 80);
        ImageLoader::load(m_thumbImage, url);
    }

    this->registerClickAction([this](brls::View*) {
        brls::Application::notify("Opening: " + m_playlist.title);
        return true;
    });
}

void PlaylistItemView::cancelPendingImage() {
    ImageLoader::cancel(m_thumbImage);
}
