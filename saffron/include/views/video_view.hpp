#ifndef SAFFRON_VIDEO_VIEW_HPP
#define SAFFRON_VIDEO_VIEW_HPP

#include <borealis.hpp>
#include "core/mpv_core.hpp"

class VideoView : public brls::View {
public:
    VideoView();
    ~VideoView() override;

    void draw(NVGcontext* ctx, float x, float y, float width, float height, brls::Style style, brls::FrameContext* frameCtx) override;
    void onLayout() override;

    void setAlpha(float alpha);

private:
    float m_alpha = 1.0f;
};

#endif
