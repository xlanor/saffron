#ifndef SAFFRON_SEARCH_VIEW_HPP
#define SAFFRON_SEARCH_VIEW_HPP

#include <borealis.hpp>
#include <vector>
#include <string>

#include "models/plex_types.hpp"

class PlexServer;
class SettingsManager;
class HubRowView;

class SearchView : public brls::Box {
public:
    SearchView(PlexServer* server, bool isPopup = true);
    SearchView(PlexServer* server, const std::string& initialQuery);
    ~SearchView() override;

    brls::View* getDefaultFocus() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    void init(bool isPopup);
    void openKeyboard();
    void performSearch(const std::string& query);
    void displayResults(const std::vector<plex::Hub>& hubs);
    void clearResults();
    void cancelPendingImages();

    BRLS_BIND(brls::Button, inputButton, "search/inputButton");
    BRLS_BIND(brls::Box, resultsContainer, "search/resultsContainer");
    BRLS_BIND(brls::Box, emptyMessage, "search/empty");
    BRLS_BIND(brls::Label, emptyLabel, "search/emptyLabel");
    BRLS_BIND(brls::Box, spinnerContainer, "search/spinner");

    PlexServer* m_server = nullptr;
    SettingsManager* m_settings = nullptr;
    std::string m_currentQuery;
    std::vector<HubRowView*> m_hubRows;

    brls::RepeatingTask* m_debounceTimer = nullptr;
    int m_searchRequestId = 0;

    static SearchView* s_currentInstance;
    static bool s_isActive;
};

#endif
