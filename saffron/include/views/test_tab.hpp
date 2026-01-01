#ifndef SAFFRON_TEST_TAB_HPP
#define SAFFRON_TEST_TAB_HPP

#include <borealis.hpp>

class TestTab : public brls::Box {
public:
    TestTab() {
        auto* label = new brls::Label();
        label->setText("Test Tab - Minimal View");
        label->setFontSize(24);
        this->addView(label);
    }

    static brls::View* create() {
        return new TestTab();
    }
};

#endif
