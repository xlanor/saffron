#pragma once

#include <borealis.hpp>
#include "view/recycling_grid.hpp"

class FocusableCard : public RecyclingGridItem {
public:
    FocusableCard();

    void onFocusGained() override;
    void onFocusLost() override;

protected:
    void registerFocusableLabel(brls::Label* label, bool isSubtitle = false);
    void registerProgressBar(brls::Box* progressBar, brls::Box* progressFill);

private:
    std::vector<brls::Label*> m_focusableLabels;
    std::vector<NVGcolor> m_originalColors;
    std::vector<bool> m_isSubtitle;

    brls::Box* m_progressBar = nullptr;
    brls::Box* m_progressFill = nullptr;

    void applyFocusedStyle();
    void restoreOriginalStyle();
};
