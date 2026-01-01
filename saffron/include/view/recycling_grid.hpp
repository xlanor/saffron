// Adapted from Switchfin (https://github.com/dragonflylee/switchfin)
// Original author: fang (2022/6/15)

#pragma once

#include <borealis.hpp>

class RecyclingView;

class RecyclingGridItem : public brls::Box {
public:
    RecyclingGridItem();
    ~RecyclingGridItem() override;

    size_t getIndex() const;
    void setIndex(size_t value);

    std::string reuseIdentifier;

    virtual void prepareForReuse() {}
    virtual void cacheForReuse() {}
    virtual void cancelPendingRequests() {}

private:
    size_t index;
};

class RecyclingGridDataSource {
public:
    virtual ~RecyclingGridDataSource() = default;

    virtual size_t getItemCount() { return 0; }
    virtual RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) { return nullptr; }
    virtual float heightForRow(brls::View* recycler, size_t index) { return -1; }
    virtual void onItemSelected(brls::Box* recycler, size_t index) {}
    virtual void clearData() = 0;
};

class RecyclingGridContentBox;

class RecyclingView {
public:
    virtual ~RecyclingView() = default;

    virtual brls::View* getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) = 0;
    virtual void setDataSource(RecyclingGridDataSource* source) = 0;

    void registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation);
    RecyclingGridItem* dequeueReusableCell(std::string identifier);
    RecyclingGridDataSource* getDataSource() const;
    void showSkeleton(unsigned int num = 12);

protected:
    void queueReusableCell(RecyclingGridItem* cell);
    void removeCell(brls::View* view);

    RecyclingGridDataSource* dataSource = nullptr;
    RecyclingGridContentBox* contentBox = nullptr;

    std::map<std::string, std::vector<RecyclingGridItem*>*> queueMap;
    std::map<std::string, std::function<RecyclingGridItem*(void)>> allocationMap;
};

class RecyclingGrid : public brls::ScrollingFrame, public RecyclingView {
public:
    RecyclingGrid();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

    void setDefaultCellFocus(size_t index);
    size_t getDefaultCellFocus() const;

    void setDataSource(RecyclingGridDataSource* source) override;
    void reloadData();
    void notifyDataChanged();

    RecyclingGridItem* getGridItemByIndex(size_t index);
    std::vector<RecyclingGridItem*>& getGridItems();

    void clearData();
    void setEmpty(std::string msg = "");
    void setError(std::string error = "");
    void cancelAllPendingImages();

    void selectRowAt(size_t index, bool animated);
    float getHeightByCellIndex(size_t index, size_t start = 0);

    View* getNextCellFocus(brls::FocusDirection direction, View* currentView) override;

    void forceRequestNextPage();
    void onLayout() override;

    size_t getItemCount();
    size_t getRowCount();

    void onNextPage(const std::function<void()>& callback = nullptr);

    void setPadding(float padding) override;
    void setPadding(float top, float right, float bottom, float left) override;
    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;

    brls::View* getDefaultFocus() override;

    ~RecyclingGrid() override;

    static View* create();

    float estimatedRowSpace = 20;
    float estimatedRowHeight = 240;
    int spanCount = 4;
    int preFetchLine = 1;
    bool isFlowMode = false;

private:
    bool layouted = false;
    float oldWidth = -1;

    bool requestNextPage = false;

    uint32_t visibleMin, visibleMax;
    size_t defaultCellFocus = 0;

    float paddingTop = 0;
    float paddingRight = 0;
    float paddingBottom = 0;
    float paddingLeft = 0;

    std::function<void()> nextPageCallback = nullptr;

    brls::Label* hintLabel;
    brls::Rect renderedFrame;
    std::vector<float> cellHeightCache;

    bool checkWidth();
    void itemsRecyclingLoop();
    void addCellAt(size_t index, bool downSide);
};

class RecyclingGridContentBox : public brls::Box {
public:
    RecyclingGridContentBox(RecyclingView* recycler);
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    RecyclingView* recycler;
};

class SkeletonCell : public RecyclingGridItem {
public:
    SkeletonCell();

    static RecyclingGridItem* create();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

private:
    NVGcolor background = nvgRGBA(60, 60, 60, 255);
};
