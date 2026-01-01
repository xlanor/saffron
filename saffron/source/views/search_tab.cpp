#include "views/search_tab.hpp"
#include "views/search_view.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "styles/plex.hpp"

SearchTab::SearchTab() {
    m_server = SettingsManager::getInstance()->getCurrentServer();

    this->setGrow(1.0f);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setAxis(brls::Axis::COLUMN);

    auto* button = new brls::Button();
    button->setText("Search");
    button->setStyle(&plex::style::BUTTONSTYLE_PLEX);
    button->setWidth(300);
    button->setMarginBottom(20);
    button->registerClickAction([this](brls::View*) {
        if (m_server) {
            auto* searchView = new SearchView(m_server, true);
            brls::Application::pushActivity(new brls::Activity(searchView));
        }
        return true;
    });
    this->addView(button);

    auto* label = new brls::Label();
    label->setText("Search by title, actor, genre, or director");
    label->setFontSize(16);
    label->setTextColor(nvgRGB(136, 136, 136));
    this->addView(label);
}

brls::View* SearchTab::create() {
    return new SearchTab();
}

void SearchTab::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    m_server = SettingsManager::getInstance()->getCurrentServer();
}
