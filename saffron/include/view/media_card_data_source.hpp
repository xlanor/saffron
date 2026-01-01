#pragma once

#include <functional>
#include <vector>

#include "view/recycling_grid.hpp"
#include "models/plex_types.hpp"

class PlexServer;

class MediaCardDataSource : public RecyclingGridDataSource {
public:
    MediaCardDataSource(PlexServer* server);

    size_t getItemCount() override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    void onItemSelected(brls::Box* recycler, size_t index) override;
    void clearData() override;

    void setData(const std::vector<plex::MediaItem>& items);
    void appendData(const std::vector<plex::MediaItem>& items);
    const plex::MediaItem& getItem(size_t index) const;

    void setOnItemClick(std::function<void(const plex::MediaItem&)> callback);

private:
    PlexServer* m_server = nullptr;
    std::vector<plex::MediaItem> m_items;
    std::function<void(const plex::MediaItem&)> m_onItemClick;
};
