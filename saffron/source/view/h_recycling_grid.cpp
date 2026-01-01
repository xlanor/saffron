#include "view/h_recycling_grid.hpp"

HRecyclingGrid::HRecyclingGrid() {
    this->setFocusable(false);
    this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    this->contentBox = new RecyclingGridContentBox(this);
    this->contentBox->setAxis(brls::Axis::ROW);
    this->setContentView(this->contentBox);
}

HRecyclingGrid::~HRecyclingGrid() {
    delete this->dataSource;
    for (const auto& it : queueMap) {
        for (auto item : *it.second) {
            item->setParent(nullptr);
            if (item->isPtrLocked())
                item->freeView();
            else
                delete item;
        }
        delete it.second;
    }
}

void HRecyclingGrid::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    itemsRecyclingLoop();
    HScrollingFrame::draw(vg, x, y, width, height, style, ctx);
}

void HRecyclingGrid::setDataSource(RecyclingGridDataSource* source) {
    if (this->dataSource) delete this->dataSource;
    this->dataSource = source;
    this->invalidate();

    if (layouted) {
        reloadData();
    }
}

void HRecyclingGrid::reloadData() {
    if (!layouted) return;

    auto& children = this->contentBox->getChildren();
    for (auto const& child : children) {
        queueReusableCell((RecyclingGridItem*)child);
        child->willDisappear(true);
    }
    children.clear();

    visibleMin = UINT32_MAX;
    visibleMax = 0;

    renderedFrame = brls::Rect();
    renderedFrame.size.height = getHeight();

    setContentOffsetX(0, false);

    if (dataSource == nullptr) return;
    if (dataSource->getItemCount() <= 0) {
        contentBox->setWidth(0);
        return;
    }

    float totalWidth = getWidthByCellIndex(dataSource->getItemCount()) + paddingLeft + paddingRight;
    contentBox->setWidth(totalWidth);

    this->addCellAt(0, true);
}

void HRecyclingGrid::clearData() {
    if (dataSource) {
        dataSource->clearData();
        this->reloadData();
    }
}

void HRecyclingGrid::cancelAllPendingImages() {
    if (!contentBox) return;
    for (brls::View* child : contentBox->getChildren()) {
        RecyclingGridItem* item = dynamic_cast<RecyclingGridItem*>(child);
        if (item) {
            item->cancelPendingRequests();
        }
    }
}

size_t HRecyclingGrid::getItemCount() {
    return this->dataSource ? this->dataSource->getItemCount() : 0;
}

float HRecyclingGrid::getWidthByCellIndex(size_t index, size_t start) {
    if (index <= start) return 0;
    return (estimatedItemWidth + estimatedItemSpace) * (index - start);
}

void HRecyclingGrid::addCellAt(size_t index, bool rightSide) {
    RecyclingGridItem* cell = dataSource->cellForRow(this, index);
    if (!cell) return;

    float cellWidth = estimatedItemWidth;
    float cellHeight = getHeight();
    float cellX = getWidthByCellIndex(index) + paddingLeft;
    float cellY = 0;

    cell->setWidth(cellWidth);
    cell->setHeight(cellHeight);
    cell->setDetachedPositionX(cellX);
    cell->setDetachedPositionY(cellY);
    cell->setIndex(index);

    this->contentBox->getChildren().insert(this->contentBox->getChildren().end(), cell);

    size_t* userdata = (size_t*)malloc(sizeof(size_t));
    *userdata = index;
    cell->setParent(this->contentBox, userdata);

    this->contentBox->invalidate();
    cell->View::willAppear();

    if (index < visibleMin) visibleMin = static_cast<uint32_t>(index);
    if (index > visibleMax) visibleMax = static_cast<uint32_t>(index);

    if (!rightSide) {
        renderedFrame.origin.x -= cellWidth + estimatedItemSpace;
    }
    renderedFrame.size.width += cellWidth + estimatedItemSpace;
}

void HRecyclingGrid::itemsRecyclingLoop() {
    if (!dataSource || dataSource->getItemCount() == 0) return;

    brls::Rect visibleFrame = getVisibleFrame();

    while (true) {
        RecyclingGridItem* minCell = nullptr;
        for (auto it : contentBox->getChildren()) {
            if (*((size_t*)it->getParentUserData()) == visibleMin) {
                minCell = (RecyclingGridItem*)it;
                break;
            }
        }

        if (!minCell) break;
        float minCellRight = minCell->getDetachedPosition().x + estimatedItemWidth;
        float prefetchWidth = (preFetchCount + 1) * (estimatedItemWidth + estimatedItemSpace);

        if (minCellRight + prefetchWidth >= visibleFrame.getMinX()) break;

        renderedFrame.origin.x += estimatedItemWidth + estimatedItemSpace;
        renderedFrame.size.width -= estimatedItemWidth + estimatedItemSpace;

        queueReusableCell(minCell);
        this->removeCell(minCell);

        visibleMin++;
    }

    while (true) {
        RecyclingGridItem* maxCell = nullptr;
        for (auto it : contentBox->getChildren()) {
            if (*((size_t*)it->getParentUserData()) == visibleMax) {
                maxCell = (RecyclingGridItem*)it;
                break;
            }
        }

        if (!maxCell) break;
        if (visibleMax == 0) break;

        float maxCellLeft = maxCell->getDetachedPosition().x;
        float prefetchWidth = preFetchCount * (estimatedItemWidth + estimatedItemSpace);

        if (maxCellLeft - prefetchWidth <= visibleFrame.getMaxX()) break;

        renderedFrame.size.width -= estimatedItemWidth + estimatedItemSpace;

        queueReusableCell(maxCell);
        this->removeCell(maxCell);

        visibleMax--;
    }

    while (visibleMin > 0 && visibleMin <= dataSource->getItemCount()) {
        float prefetchWidth = preFetchCount * (estimatedItemWidth + estimatedItemSpace);
        if (renderedFrame.getMinX() + prefetchWidth < visibleFrame.getMinX() - paddingLeft) {
            break;
        }
        addCellAt(visibleMin - 1, false);
    }

    while (visibleMax + 1 < dataSource->getItemCount()) {
        float prefetchWidth = preFetchCount * (estimatedItemWidth + estimatedItemSpace);
        if (renderedFrame.getMaxX() - prefetchWidth > visibleFrame.getMaxX() - paddingRight) {
            break;
        }
        addCellAt(visibleMax + 1, true);
    }
}

brls::View* HRecyclingGrid::getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) {
    void* parentUserData = currentView->getParentUserData();
    size_t currentIndex = *((size_t*)parentUserData);

    if (direction == brls::FocusDirection::UP || direction == brls::FocusDirection::DOWN) {
        brls::View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
        return next;
    }

    if (direction == brls::FocusDirection::LEFT && currentIndex == 0) {
        brls::View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
        return next;
    }

    if (direction == brls::FocusDirection::RIGHT && currentIndex >= dataSource->getItemCount() - 1) {
        brls::View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
        return next;
    }

    int offset = (direction == brls::FocusDirection::LEFT) ? -1 : 1;
    size_t targetIndex = currentIndex + offset;

    brls::View* targetFocus = nullptr;
    for (auto it : this->contentBox->getChildren()) {
        if (*((size_t*)it->getParentUserData()) == targetIndex) {
            targetFocus = it->getDefaultFocus();
            break;
        }
    }

    if (targetFocus) {
        itemsRecyclingLoop();
        return targetFocus;
    }

    targetFocus = getParentNavigationDecision(this, nullptr, direction);
    if (!targetFocus && hasParent()) targetFocus = getParent()->getNextFocus(direction, this);
    return targetFocus;
}

brls::View* HRecyclingGrid::getDefaultFocus() {
    if (!this->dataSource || this->dataSource->getItemCount() == 0) return nullptr;

    for (auto it : this->contentBox->getChildren()) {
        if (*((size_t*)it->getParentUserData()) == 0) {
            return it->getDefaultFocus();
        }
    }
    return nullptr;
}

void HRecyclingGrid::onLayout() {
    HScrollingFrame::onLayout();

    auto rect = this->getFrame();
    float height = rect.getHeight();
    float width = rect.getWidth();

    if (height != height || width != width) return;
    if (height <= 0 || width <= 0) return;
    if (!this->contentBox) return;

    this->contentBox->setHeight(height);

    if (!layouted) {
        layouted = true;
        if (dataSource && dataSource->getItemCount() > 0) {
            reloadData();
        }
    }
}
