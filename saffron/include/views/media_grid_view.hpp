#ifndef SAFFRON_MEDIA_GRID_VIEW_HPP
#define SAFFRON_MEDIA_GRID_VIEW_HPP

#include <borealis.hpp>
#include <vector>

#include "models/plex_types.hpp"
#include "view/focusable_card.hpp"

class PlexServer;

class MediaCardView;

class MediaGridView : public brls::Box {
public:
    MediaGridView();
    ~MediaGridView() override;

    void setServer(PlexServer* server);
    void setItems(const std::vector<plex::MediaItem>& items);
    void appendItems(const std::vector<plex::MediaItem>& items);
    void clearItems();

    void setOnItemClick(std::function<void(const plex::MediaItem&)> callback);
    void setOnLoadMore(std::function<void()> callback);

    void setTotalItems(int total) { m_totalItems = total; }
    int getTotalItems() const { return m_totalItems; }
    int getLoadedCount() const { return static_cast<int>(m_cards.size()); }
    bool hasMoreItems() const { return m_totalItems > 0 && getLoadedCount() < m_totalItems; }

private:
    PlexServer* m_server = nullptr;
    std::vector<MediaCardView*> m_cards;
    std::function<void(const plex::MediaItem&)> m_onItemClick;
    std::function<void()> m_onLoadMore;

    brls::Box* m_gridContainer = nullptr;
    brls::Box* m_currentRow = nullptr;
    int m_itemsPerRow = 5;
    int m_totalItems = 0;
    bool m_isLoading = false;

    std::vector<plex::MediaItem> m_pendingItems;
    size_t m_batchTimerId = 0;
    static constexpr int ITEMS_PER_BATCH = 10;

    void addItemToGrid(const plex::MediaItem& item);
    void createNewRow();
    void checkLoadMore();
    void processBatch();
    void cancelPendingBatches();
};

class MediaCardView : public FocusableCard {
public:
    MediaCardView();

    void setData(PlexServer* server, const plex::MediaItem& item);
    void prepareForReuse() override;
    void cacheForReuse() override;
    void cancelPendingRequests() override;

    void setOnClick(std::function<void()> callback);
    const plex::MediaItem& getItem() const { return m_item; }

    static RecyclingGridItem* create();

private:
    PlexServer* m_server = nullptr;
    plex::MediaItem m_item;

    brls::Image* m_posterImage = nullptr;
    brls::Label* m_titleLabel = nullptr;
    brls::Label* m_subtitleLabel = nullptr;
    brls::Box* m_textContainer = nullptr;
    brls::Box* m_progressBar = nullptr;
    brls::Box* m_progressFill = nullptr;

    void loadPosterImage();
    std::string buildImageUrl();
};

#endif
