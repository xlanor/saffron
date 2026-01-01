#pragma once

#include <borealis.hpp>

namespace plex {
namespace style {

inline NVGcolor orange() { return nvgRGBA(229, 160, 13, 255); }
inline NVGcolor orangeDark() { return nvgRGBA(204, 143, 11, 255); }

inline const brls::ButtonStyle BUTTONSTYLE_PLEX = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,

    .highlightPadding = "brls/button/primary_highlight_padding",
    .borderThickness  = "",

    .enabledBackgroundColor = "plex/button/background",
    .enabledLabelColor      = "plex/button/text",
    .enabledBorderColor     = "",

    .disabledBackgroundColor = "plex/button/background_disabled",
    .disabledLabelColor      = "plex/button/text_disabled",
    .disabledBorderColor     = "",
};

inline void applyPlayButton(brls::Button* button) {
    button->setStyle(&BUTTONSTYLE_PLEX);
    button->setWidth(200);
    button->setHeight(45);
    button->setFontSize(18);
}

}
}
