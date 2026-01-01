#ifndef SAFFRON_TAG_MEDIA_VIEW_HPP
#define SAFFRON_TAG_MEDIA_VIEW_HPP

#include <borealis.hpp>
#include <vector>
#include <string>

#include "models/plex_types.hpp"
#include "view/recycling_grid.hpp"

class PlexServer;

enum class TagType {
    Actor,
    Genre,
    Director,
    Writer,
    Collection
};

class TagMediaView : public brls::Box {
public:
    TagMediaView(PlexServer* server, TagType type, int tagId,
                 const std::string& tagName, const std::string& tagThumb = "",
                 int librarySectionId = 0);
    ~TagMediaView() override;

    brls::View* getDefaultFocus() override;
    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

private:
    void setupUI();
    void loadMedia();
    void appendMedia(const std::vector<plex::MediaItem>& items);

    std::string getTagTypeLabel() const;
    std::string buildApiEndpoint() const;

    PlexServer* m_server;
    TagType m_tagType;
    int m_tagId;
    std::string m_tagName;
    std::string m_tagThumb;
    int m_librarySectionId;

    brls::Image* m_tagImage = nullptr;
    brls::Box* m_tagImageContainer = nullptr;
    brls::Label* m_nameLabel = nullptr;
    brls::Label* m_typeLabel = nullptr;
    brls::Label* m_countLabel = nullptr;
    RecyclingGrid* m_recyclerGrid = nullptr;
    brls::Box* m_spinnerContainer = nullptr;
    brls::Label* m_emptyLabel = nullptr;
    brls::Box* m_focusPlaceholder = nullptr;

    int m_totalItems = 0;
    int m_loadedItems = 0;
    bool m_isLoading = false;
    bool m_dataLoaded = false;
    int m_requestId = 0;

    static TagMediaView* s_instance;
};

class TagMediaDataSource : public RecyclingGridDataSource {
public:
    TagMediaDataSource(PlexServer* server, std::function<void(const plex::MediaItem&)> onItemClick);

    size_t getItemCount() override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    void onItemSelected(brls::Box* recycler, size_t index) override;
    void clearData() override;

    void appendItems(const std::vector<plex::MediaItem>& items);

private:
    PlexServer* m_server;
    std::vector<plex::MediaItem> m_items;
    std::function<void(const plex::MediaItem&)> m_onItemClick;
};

#endif
