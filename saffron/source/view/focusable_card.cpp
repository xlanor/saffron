#include "view/focusable_card.hpp"

FocusableCard::FocusableCard() {
    this->setHideHighlightBackground(true);
}

void FocusableCard::registerFocusableLabel(brls::Label* label, bool isSubtitle) {
    m_focusableLabels.push_back(label);
    m_originalColors.push_back(label->getTextColor());
    m_isSubtitle.push_back(isSubtitle);
}

void FocusableCard::registerProgressBar(brls::Box* progressBar, brls::Box* progressFill) {
    m_progressBar = progressBar;
    m_progressFill = progressFill;
}

void FocusableCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    applyFocusedStyle();
}

void FocusableCard::onFocusLost() {
    RecyclingGridItem::onFocusLost();
    restoreOriginalStyle();
}

void FocusableCard::applyFocusedStyle() {
    this->setBackgroundColor(nvgRGBA(229, 160, 13, 255));

    NVGcolor titleColor = nvgRGB(0, 0, 0);
    NVGcolor subtitleColor = nvgRGBA(60, 60, 60, 255);

    for (size_t i = 0; i < m_focusableLabels.size(); i++) {
        m_focusableLabels[i]->setTextColor(m_isSubtitle[i] ? subtitleColor : titleColor);
    }

    if (m_progressFill) m_progressFill->setBackgroundColor(nvgRGBA(0, 0, 0, 255));
    if (m_progressBar) m_progressBar->setBackgroundColor(nvgRGBA(0, 0, 0, 80));
}

void FocusableCard::restoreOriginalStyle() {
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));

    for (size_t i = 0; i < m_focusableLabels.size(); i++) {
        m_focusableLabels[i]->setTextColor(m_originalColors[i]);
    }

    if (m_progressFill) m_progressFill->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
    if (m_progressBar) m_progressBar->setBackgroundColor(nvgRGBA(80, 80, 80, 255));
}
