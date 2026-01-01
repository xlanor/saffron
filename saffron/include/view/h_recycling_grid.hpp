#pragma once

#include <borealis.hpp>
#include "view/recycling_grid.hpp"

class HRecyclingGrid : public brls::HScrollingFrame, public RecyclingView {
public:
    HRecyclingGrid();
    ~HRecyclingGrid() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

    void setDataSource(RecyclingGridDataSource* source) override;
    void reloadData();

    void clearData();
    void cancelAllPendingImages();

    brls::View* getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) override;
    brls::View* getDefaultFocus() override;

    void onLayout() override;

    size_t getItemCount();

    float estimatedItemWidth = 200;
    float estimatedItemSpace = 20;
    int preFetchCount = 2;

private:
    bool layouted = false;

    uint32_t visibleMin = UINT32_MAX;
    uint32_t visibleMax = 0;

    float paddingLeft = 0;
    float paddingRight = 0;

    brls::Rect renderedFrame;

    void itemsRecyclingLoop();
    void addCellAt(size_t index, bool rightSide);
    float getWidthByCellIndex(size_t index, size_t start = 0);
};
