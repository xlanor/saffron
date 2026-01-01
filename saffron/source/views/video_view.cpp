#include "views/video_view.hpp"

VideoView::VideoView() {
    setFocusable(false);
}

VideoView::~VideoView() {
}

void VideoView::draw(NVGcontext* ctx, float x, float y, float width, float height,
                     brls::Style style, brls::FrameContext* frameCtx) {
    auto* mpv = MPVCore::getInstance();
    if (mpv->isValid()) {
        brls::Rect rect = {x, y, width, height};
        mpv->draw(rect, m_alpha);

        int64_t pos = mpv->getPosition();
        int64_t dur = mpv->getDuration();
        float progress = (dur > 0) ? static_cast<float>(pos) / static_cast<float>(dur) : 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        nvgBeginPath(ctx);
        nvgFillColor(ctx, nvgRGBA(230, 120, 30, 200));
        nvgRect(ctx, x, y + height - 3, width * progress, 3);
        nvgFill(ctx);
    }
}

void VideoView::onLayout() {
    brls::View::onLayout();

    auto* mpv = MPVCore::getInstance();
    if (mpv->isValid()) {
        brls::Rect rect = getFrame();
        mpv->setFrameSize(rect);
    }
}

void VideoView::setAlpha(float alpha) {
    m_alpha = alpha;
}
