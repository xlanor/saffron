#ifndef SAFFRON_PLEX_API_HPP
#define SAFFRON_PLEX_API_HPP

#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "models/plex_types.hpp"

class PlexServer;

struct PlexHeaders {
    std::string clientIdentifier;
    std::string product = "Saffron";
    std::string version = "0.1.0";
    std::string platform = "Android";
    std::string platformVersion = "11";
    std::string device = "SHIELD Android TV";
    std::string model = "SHIELD Android TV";
    std::string token;
    std::string sessionIdentifier;
    std::string clientProfileName = "SHIELD Android TV";
    std::string clientProfileExtra;

    std::vector<std::string> toHeaderList() const;
};

class PlexApi {
public:
    using OnError = std::function<void(const std::string&)>;

    static void getLibrarySections(
        PlexServer* server,
        std::function<void(std::vector<plex::Library>)> onSuccess,
        OnError onError
    );

    static void getLibraryItems(
        PlexServer* server,
        int sectionId,
        int start,
        int count,
        std::function<void(std::vector<plex::MediaItem>, int totalSize)> onSuccess,
        OnError onError
    );

    static void getRecentlyAdded(
        PlexServer* server,
        int sectionId,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getMetadata(
        PlexServer* server,
        int ratingKey,
        std::function<void(plex::MediaItem)> onSuccess,
        OnError onError
    );

    static void getChildren(
        PlexServer* server,
        int ratingKey,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getHubs(
        PlexServer* server,
        std::function<void(std::vector<plex::Hub>)> onSuccess,
        OnError onError
    );

    static void getContinueWatching(
        PlexServer* server,
        std::function<void(plex::Hub)> onSuccess,
        OnError onError
    );

    static void getOnDeck(
        PlexServer* server,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getCollections(
        PlexServer* server,
        int sectionId,
        std::function<void(std::vector<plex::Collection>)> onSuccess,
        OnError onError
    );

    static void getCollectionItems(
        PlexServer* server,
        int collectionId,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getPlaylists(
        PlexServer* server,
        std::function<void(std::vector<plex::Playlist>)> onSuccess,
        OnError onError
    );

    static void getPlaylistItems(
        PlexServer* server,
        int playlistId,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getPlaybackDecision(
        PlexServer* server,
        int ratingKey,
        int maxBitrate,
        const std::string& resolution,
        bool forceTranscode,
        int64_t offsetMs,
        int mediaIndex,
        std::function<void(plex::PlaybackInfo)> onSuccess,
        OnError onError
    );

    static void reportTimeline(
        PlexServer* server,
        int ratingKey,
        int64_t time,
        const std::string& state,
        int64_t duration,
        const std::string& sessionId = "",
        OnError onError = nullptr
    );

    static void search(
        PlexServer* server,
        const std::string& query,
        int limit,
        std::function<void(std::vector<plex::Hub>)> onSuccess,
        OnError onError
    );

    static void getPersonMedia(
        PlexServer* server,
        int personId,
        std::function<void(std::vector<plex::MediaItem>)> onSuccess,
        OnError onError
    );

    static void getTagMedia(
        PlexServer* server,
        const std::string& endpoint,
        int start,
        int count,
        std::function<void(std::vector<plex::MediaItem>, int totalSize)> onSuccess,
        OnError onError
    );

private:
    static PlexHeaders buildHeaders();
    static std::string buildUrl(PlexServer* server, const std::string& path);

    static void get(
        const std::string& url,
        const PlexHeaders& headers,
        std::function<void(const nlohmann::json&)> onSuccess,
        OnError onError
    );
};

#endif
