#ifndef SAFFRON_SEARCH_TAB_HPP
#define SAFFRON_SEARCH_TAB_HPP

#include <borealis.hpp>

class PlexServer;

class SearchTab : public brls::Box {
public:
    SearchTab();

    static brls::View* create();

    void willAppear(bool resetState) override;

private:
    PlexServer* m_server = nullptr;
};

#endif
