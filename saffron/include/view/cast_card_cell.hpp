#pragma once

#include <borealis.hpp>
#include "view/focusable_card.hpp"
#include "models/plex_types.hpp"

class PlexServer;

class CastCardCell : public FocusableCard {
public:
    CastCardCell();

    static RecyclingGridItem* create();

    void setActor(const plex::Tag& actor, PlexServer* server);
    void prepareForReuse() override;
    void cacheForReuse() override;
    void cancelPendingRequests() override;

private:
    brls::Image* m_image = nullptr;
    brls::Label* m_nameLabel = nullptr;
    brls::Label* m_roleLabel = nullptr;
};

class CastDataSource : public RecyclingGridDataSource {
public:
    CastDataSource(const std::vector<plex::Tag>& cast, PlexServer* server);

    size_t getItemCount() override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    void onItemSelected(brls::Box* recycler, size_t index) override;
    void clearData() override;

    void setOnActorClick(std::function<void(const plex::Tag&)> callback);

private:
    std::vector<plex::Tag> m_cast;
    PlexServer* m_server;
    std::function<void(const plex::Tag&)> m_onActorClick;
};
