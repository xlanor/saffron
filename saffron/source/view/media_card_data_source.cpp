#include "view/media_card_data_source.hpp"
#include "views/media_grid_view.hpp"

MediaCardDataSource::MediaCardDataSource(PlexServer* server)
    : m_server(server) {
}

size_t MediaCardDataSource::getItemCount() {
    return m_items.size();
}

RecyclingGridItem* MediaCardDataSource::cellForRow(RecyclingView* recycler, size_t index) {
    MediaCardView* cell = dynamic_cast<MediaCardView*>(
        recycler->dequeueReusableCell("MediaCard"));

    if (index < m_items.size()) {
        cell->setData(m_server, m_items[index]);
    }

    return cell;
}

void MediaCardDataSource::onItemSelected(brls::Box* recycler, size_t index) {
    if (m_onItemClick && index < m_items.size()) {
        m_onItemClick(m_items[index]);
    }
}

void MediaCardDataSource::clearData() {
    m_items.clear();
}

void MediaCardDataSource::setData(const std::vector<plex::MediaItem>& items) {
    m_items = items;
}

void MediaCardDataSource::appendData(const std::vector<plex::MediaItem>& items) {
    m_items.insert(m_items.end(), items.begin(), items.end());
}

const plex::MediaItem& MediaCardDataSource::getItem(size_t index) const {
    return m_items[index];
}

void MediaCardDataSource::setOnItemClick(std::function<void(const plex::MediaItem&)> callback) {
    m_onItemClick = callback;
}
