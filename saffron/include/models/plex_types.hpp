#ifndef SAFFRON_PLEX_TYPES_HPP
#define SAFFRON_PLEX_TYPES_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace plex {

enum class MediaType : int {
    Movie = 1,
    Show = 2,
    Season = 3,
    Episode = 4,
    Trailer = 5,
    Comic = 6,
    Person = 7,
    Artist = 8,
    Album = 9,
    Track = 10,
    Clip = 12,
    Photo = 13,
    PhotoAlbum = 14,
    Playlist = 15,
    PlaylistFolder = 16,
    Collection = 18,
    Genre = 100,
    Director = 101,
    Writer = 102
};

struct Stream {
    int id = 0;
    int streamType = 0;
    std::string codec;
    std::string language;
    std::string languageCode;
    std::string displayTitle;
    bool selected = false;
    bool isDefault = false;
    std::string decision;
    int index = 0;
    int channels = 0;
    int bitrate = 0;
    std::string profile;
    std::string audioChannelLayout;
    int bitDepth = 0;
    std::string colorSpace;
    std::string colorRange;
    std::string colorPrimaries;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
};

struct Part {
    int id = 0;
    std::string key;
    int64_t duration = 0;
    std::string file;
    int64_t size = 0;
    std::string container;
    std::string videoProfile;
    std::string audioProfile;
    std::vector<Stream> streams;
};

struct Media {
    int id = 0;
    int64_t duration = 0;
    int bitrate = 0;
    int width = 0;
    int height = 0;
    std::string aspectRatio;
    int audioChannels = 0;
    std::string audioCodec;
    std::string videoCodec;
    std::string videoResolution;
    std::string container;
    std::string videoFrameRate;
    std::string videoProfile;
    std::string audioProfile;
    std::string editionTitle;
    bool optimizedForStreaming = false;
    std::vector<Part> parts;
};

struct Tag {
    int id = 0;
    std::string tag;
    std::string role;
    std::string thumb;
};

struct MediaItem {
    int ratingKey = 0;
    std::string key;
    std::string guid;
    std::string type;
    MediaType mediaType = MediaType::Movie;
    std::string title;
    std::string titleSort;
    std::string editionTitle;
    std::string summary;
    std::string thumb;
    std::string art;
    std::string banner;
    int64_t duration = 0;
    int64_t addedAt = 0;
    int64_t updatedAt = 0;
    int year = 0;
    std::string contentRating;
    double rating = 0.0;
    double audienceRating = 0.0;
    std::string audienceRatingImage;
    std::string studio;
    std::string tagline;
    std::string originalTitle;
    std::string originallyAvailableAt;

    int64_t viewOffset = 0;
    int viewCount = 0;
    int64_t lastViewedAt = 0;

    int parentRatingKey = 0;
    int grandparentRatingKey = 0;
    int index = 0;
    int parentIndex = 0;
    std::string grandparentTitle;
    std::string parentTitle;
    std::string grandparentThumb;
    std::string parentThumb;

    int leafCount = 0;
    int viewedLeafCount = 0;
    int childCount = 0;

    int librarySectionId = 0;

    std::vector<Media> media;
    std::vector<Tag> genres;
    std::vector<Tag> countries;
    std::vector<Tag> directors;
    std::vector<Tag> writers;
    std::vector<Tag> cast;
};

struct Library {
    int key = 0;
    std::string uuid;
    std::string type;
    std::string title;
    std::string art;
    std::string thumb;
    std::string composite;
    bool allowSync = false;
    int64_t updatedAt = 0;
    int64_t scannedAt = 0;
};

struct Hub {
    std::string hubKey;
    std::string key;
    std::string title;
    std::string type;
    std::string hubIdentifier;
    std::string context;
    int size = 0;
    bool more = false;
    std::string style;
    bool promoted = false;
    std::vector<MediaItem> items;
};

struct Collection {
    int ratingKey = 0;
    std::string key;
    std::string guid;
    std::string type;
    std::string title;
    std::string summary;
    std::string thumb;
    std::string art;
    int childCount = 0;
    int64_t addedAt = 0;
    int64_t updatedAt = 0;
};

struct Playlist {
    int ratingKey = 0;
    std::string key;
    std::string guid;
    std::string type;
    std::string title;
    std::string summary;
    std::string thumb;
    std::string composite;
    int leafCount = 0;
    int64_t duration = 0;
    bool smart = false;
    std::string playlistType;
    int64_t addedAt = 0;
    int64_t updatedAt = 0;
};

struct PlaybackInfo {
    std::string generalDecisionText;
    int generalDecisionCode = 0;
    std::string directPlayDecisionText;
    int directPlayDecisionCode = 0;
    std::string transcodeDecisionText;
    int transcodeDecisionCode = 0;
    bool directPlayable = false;
    std::string playbackUrl;
    std::string protocol;
    std::string sessionId;
    MediaItem metadata;
};

struct TranscodeSession {
    std::string key;
    int progress = 0;
    double speed = 0.0;
    int64_t duration = 0;
    int64_t remaining = 0;
    std::string context;
    std::string sourceVideoCodec;
    std::string sourceAudioCodec;
    std::string videoCodec;
    std::string audioCodec;
    int width = 0;
    int height = 0;
    bool transcodeHwRequested = false;
};

void from_json(const nlohmann::json& j, Stream& s);
void from_json(const nlohmann::json& j, Part& p);
void from_json(const nlohmann::json& j, Media& m);
void from_json(const nlohmann::json& j, Tag& t);
void from_json(const nlohmann::json& j, MediaItem& item);
void from_json(const nlohmann::json& j, Library& lib);
void from_json(const nlohmann::json& j, Hub& hub);
void from_json(const nlohmann::json& j, Collection& col);
void from_json(const nlohmann::json& j, Playlist& pl);

}

#endif
