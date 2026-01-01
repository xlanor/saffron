#include "views/search_view.hpp"
#include "views/home_tab.hpp"
#include "views/media_detail_view.hpp"
#include "views/show_detail_view.hpp"
#include "views/tag_media_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"
#include "styles/plex.hpp"

SearchView* SearchView::s_currentInstance = nullptr;
bool SearchView::s_isActive = false;

SearchView::SearchView(PlexServer* server, bool isPopup) : m_server(server) {
    init(isPopup);
}

SearchView::SearchView(PlexServer* server, const std::string& initialQuery) : m_server(server) {
    init(true);
    m_currentQuery = initialQuery;
    if (inputButton) inputButton->setText(initialQuery);
}

void SearchView::init(bool isPopup) {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/search.xml");
    m_settings = SettingsManager::getInstance();

    if (isPopup) {
        this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
    }

    if (inputButton) {
        inputButton->setStyle(&plex::style::BUTTONSTYLE_PLEX);
        inputButton->registerClickAction([this](brls::View*) {
            openKeyboard();
            return true;
        });
    }
}

SearchView::~SearchView() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
    if (m_debounceTimer) {
        m_debounceTimer->stop();
        delete m_debounceTimer;
    }
    m_hubRows.clear();
}

brls::View* SearchView::getDefaultFocus() {
    if (inputButton) return inputButton;
    return this;
}

void SearchView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;

    if (!m_currentQuery.empty() && m_hubRows.empty()) {
        performSearch(m_currentQuery);
    } else if (m_hubRows.empty() && m_currentQuery.empty()) {
        if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        if (emptyLabel) emptyLabel->setText("Search by title, actor, genre, or director");
    }
}

void SearchView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;
    cancelPendingImages();
}

void SearchView::cancelPendingImages() {
    for (HubRowView* row : m_hubRows) {
        row->cancelPendingImages();
    }
}

void SearchView::openKeyboard() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (text.empty()) return;

            m_currentQuery = text;
            if (inputButton) inputButton->setText(text);
            performSearch(text);
        },
        "Search",
        "Enter search query",
        256,
        m_currentQuery
    );
}

void SearchView::performSearch(const std::string& query) {
    if (!m_server) {
        if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        if (emptyLabel) emptyLabel->setText("No server selected");
        return;
    }

    if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::VISIBLE);
    if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::GONE);

    int requestId = ++m_searchRequestId;

    PlexApi::search(
        m_server,
        query,
        10,
        [this, requestId](std::vector<plex::Hub> hubs) {
            if (!s_isActive || s_currentInstance != this) return;
            if (requestId != m_searchRequestId) return;

            displayResults(hubs);
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
        },
        [this, requestId](const std::string& error) {
            if (!s_isActive || s_currentInstance != this) return;
            if (requestId != m_searchRequestId) return;

            brls::Logger::error("Search failed: {}", error);
            if (spinnerContainer) spinnerContainer->setVisibility(brls::Visibility::GONE);
            if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::VISIBLE);
            if (emptyLabel) emptyLabel->setText("Search failed");
        }
    );
}

void SearchView::displayResults(const std::vector<plex::Hub>& hubs) {
    clearResults();

    if (!m_server) return;

    for (const auto& hub : hubs) {
        if (hub.items.empty()) continue;

        auto* hubRow = new HubRowView(m_server, hub);
        hubRow->setOnItemClick([this](const plex::MediaItem& item) {
            if (item.mediaType == plex::MediaType::Person) {
                auto* tagView = new TagMediaView(m_server, TagType::Actor, item.ratingKey, item.title, item.thumb);
                brls::Application::pushActivity(new brls::Activity(tagView));
            } else if (item.mediaType == plex::MediaType::Genre) {
                auto* tagView = new TagMediaView(m_server, TagType::Genre, item.ratingKey, item.title, item.thumb, item.librarySectionId);
                brls::Application::pushActivity(new brls::Activity(tagView));
            } else if (item.mediaType == plex::MediaType::Director) {
                auto* tagView = new TagMediaView(m_server, TagType::Director, item.ratingKey, item.title, item.thumb, item.librarySectionId);
                brls::Application::pushActivity(new brls::Activity(tagView));
            } else if (item.mediaType == plex::MediaType::Writer) {
                auto* tagView = new TagMediaView(m_server, TagType::Writer, item.ratingKey, item.title, item.thumb, item.librarySectionId);
                brls::Application::pushActivity(new brls::Activity(tagView));
            } else if (item.mediaType == plex::MediaType::Show) {
                auto* showView = new ShowDetailView(m_server, item);
                brls::Application::pushActivity(new brls::Activity(showView));
            } else {
                auto* detailView = new MediaDetailView(m_server, item);
                brls::Application::pushActivity(new brls::Activity(detailView));
            }
        });

        if (resultsContainer) {
            resultsContainer->addView(hubRow);
        }
        m_hubRows.push_back(hubRow);
    }

    if (m_hubRows.empty()) {
        if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::VISIBLE);
        if (emptyLabel) emptyLabel->setText("No results found");
    } else {
        if (emptyMessage) emptyMessage->setVisibility(brls::Visibility::GONE);
    }
}

void SearchView::clearResults() {
    if (resultsContainer) {
        resultsContainer->clearViews();
    }
    m_hubRows.clear();
}
