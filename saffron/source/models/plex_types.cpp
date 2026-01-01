#include "models/plex_types.hpp"

namespace plex {

static std::string safeGetString(const nlohmann::json& j, const std::string& key) {
    if (!j.contains(key)) return "";
    const auto& val = j[key];
    if (val.is_string()) return val.get<std::string>();
    if (val.is_number_integer()) return std::to_string(val.get<int64_t>());
    if (val.is_number_float()) return std::to_string(val.get<double>());
    return "";
}

static int safeGetInt(const nlohmann::json& j, const std::string& key) {
    if (!j.contains(key)) return 0;
    const auto& val = j[key];
    if (val.is_number_integer()) return val.get<int>();
    if (val.is_string()) {
        try { return std::stoi(val.get<std::string>()); }
        catch (...) { return 0; }
    }
    return 0;
}

static bool safeGetBool(const nlohmann::json& j, const std::string& key) {
    if (!j.contains(key)) return false;
    const auto& val = j[key];
    if (val.is_boolean()) return val.get<bool>();
    if (val.is_number_integer()) return val.get<int>() != 0;
    if (val.is_string()) return val.get<std::string>() == "1" || val.get<std::string>() == "true";
    return false;
}

static double safeGetDouble(const nlohmann::json& j, const std::string& key) {
    if (!j.contains(key)) return 0.0;
    const auto& val = j[key];
    if (val.is_number()) return val.get<double>();
    if (val.is_string()) {
        try { return std::stod(val.get<std::string>()); }
        catch (...) { return 0.0; }
    }
    return 0.0;
}

void from_json(const nlohmann::json& j, Stream& s) {
    s.id = safeGetInt(j, "id");
    s.streamType = safeGetInt(j, "streamType");
    s.codec = safeGetString(j, "codec");
    s.language = safeGetString(j, "language");
    s.languageCode = safeGetString(j, "languageCode");
    s.displayTitle = safeGetString(j, "displayTitle");
    s.selected = safeGetBool(j, "selected");
    s.isDefault = safeGetBool(j, "default");
    s.decision = safeGetString(j, "decision");
    s.index = safeGetInt(j, "index");
    s.channels = safeGetInt(j, "channels");
    s.bitrate = safeGetInt(j, "bitrate");
    s.profile = safeGetString(j, "profile");
    s.audioChannelLayout = safeGetString(j, "audioChannelLayout");
    s.bitDepth = safeGetInt(j, "bitDepth");
    s.colorSpace = safeGetString(j, "colorSpace");
    s.colorRange = safeGetString(j, "colorRange");
    s.colorPrimaries = safeGetString(j, "colorPrimaries");
    s.width = safeGetInt(j, "width");
    s.height = safeGetInt(j, "height");
    s.frameRate = safeGetDouble(j, "frameRate");
}

void from_json(const nlohmann::json& j, Part& p) {
    p.id = safeGetInt(j, "id");
    p.key = safeGetString(j, "key");
    if (j.contains("duration")) p.duration = j["duration"].get<int64_t>();
    p.file = safeGetString(j, "file");
    if (j.contains("size")) p.size = j["size"].get<int64_t>();
    p.container = safeGetString(j, "container");
    p.videoProfile = safeGetString(j, "videoProfile");
    p.audioProfile = safeGetString(j, "audioProfile");
    if (j.contains("Stream")) {
        for (const auto& stream : j["Stream"]) {
            Stream s;
            from_json(stream, s);
            p.streams.push_back(s);
        }
    }
}

void from_json(const nlohmann::json& j, Media& m) {
    m.id = safeGetInt(j, "id");
    if (j.contains("duration")) m.duration = j["duration"].get<int64_t>();
    m.bitrate = safeGetInt(j, "bitrate");
    m.width = safeGetInt(j, "width");
    m.height = safeGetInt(j, "height");
    m.aspectRatio = safeGetString(j, "aspectRatio");
    m.audioChannels = safeGetInt(j, "audioChannels");
    m.audioCodec = safeGetString(j, "audioCodec");
    m.videoCodec = safeGetString(j, "videoCodec");
    m.videoResolution = safeGetString(j, "videoResolution");
    m.container = safeGetString(j, "container");
    m.videoFrameRate = safeGetString(j, "videoFrameRate");
    m.videoProfile = safeGetString(j, "videoProfile");
    m.audioProfile = safeGetString(j, "audioProfile");
    m.editionTitle = safeGetString(j, "editionTitle");
    m.optimizedForStreaming = safeGetBool(j, "optimizedForStreaming");
    if (j.contains("Part")) {
        for (const auto& part : j["Part"]) {
            Part p;
            from_json(part, p);
            m.parts.push_back(p);
        }
    }
}

void from_json(const nlohmann::json& j, Tag& t) {
    t.id = safeGetInt(j, "id");
    t.tag = safeGetString(j, "tag");
    t.role = safeGetString(j, "role");
    t.thumb = safeGetString(j, "thumb");
}

void from_json(const nlohmann::json& j, MediaItem& item) {
    item.ratingKey = safeGetInt(j, "ratingKey");
    item.key = safeGetString(j, "key");
    item.guid = safeGetString(j, "guid");
    item.type = safeGetString(j, "type");

    if (item.type == "movie") item.mediaType = MediaType::Movie;
    else if (item.type == "show") item.mediaType = MediaType::Show;
    else if (item.type == "season") item.mediaType = MediaType::Season;
    else if (item.type == "episode") item.mediaType = MediaType::Episode;
    else if (item.type == "person") item.mediaType = MediaType::Person;

    item.title = safeGetString(j, "title");
    item.titleSort = safeGetString(j, "titleSort");
    item.editionTitle = safeGetString(j, "editionTitle");
    item.summary = safeGetString(j, "summary");
    item.thumb = safeGetString(j, "thumb");
    item.art = safeGetString(j, "art");
    item.banner = safeGetString(j, "banner");
    if (j.contains("duration")) item.duration = j["duration"].get<int64_t>();
    if (j.contains("addedAt")) item.addedAt = j["addedAt"].get<int64_t>();
    if (j.contains("updatedAt")) item.updatedAt = j["updatedAt"].get<int64_t>();
    item.year = safeGetInt(j, "year");
    item.contentRating = safeGetString(j, "contentRating");
    if (j.contains("rating")) item.rating = j["rating"].get<double>();
    if (j.contains("audienceRating")) item.audienceRating = j["audienceRating"].get<double>();
    item.audienceRatingImage = safeGetString(j, "audienceRatingImage");
    item.studio = safeGetString(j, "studio");
    item.tagline = safeGetString(j, "tagline");
    item.originalTitle = safeGetString(j, "originalTitle");
    item.originallyAvailableAt = safeGetString(j, "originallyAvailableAt");

    if (j.contains("viewOffset")) item.viewOffset = j["viewOffset"].get<int64_t>();
    item.viewCount = safeGetInt(j, "viewCount");
    if (j.contains("lastViewedAt")) item.lastViewedAt = j["lastViewedAt"].get<int64_t>();

    item.parentRatingKey = safeGetInt(j, "parentRatingKey");
    item.grandparentRatingKey = safeGetInt(j, "grandparentRatingKey");
    item.index = safeGetInt(j, "index");
    item.parentIndex = safeGetInt(j, "parentIndex");
    item.grandparentTitle = safeGetString(j, "grandparentTitle");
    item.parentTitle = safeGetString(j, "parentTitle");
    item.grandparentThumb = safeGetString(j, "grandparentThumb");
    item.parentThumb = safeGetString(j, "parentThumb");

    item.leafCount = safeGetInt(j, "leafCount");
    item.viewedLeafCount = safeGetInt(j, "viewedLeafCount");
    item.childCount = safeGetInt(j, "childCount");
    item.librarySectionId = safeGetInt(j, "librarySectionID");

    if (j.contains("Media")) {
        for (const auto& media : j["Media"]) {
            Media m;
            from_json(media, m);
            item.media.push_back(m);
        }
    }

    if (j.contains("Genre")) {
        for (const auto& tag : j["Genre"]) {
            Tag t;
            from_json(tag, t);
            item.genres.push_back(t);
        }
    }

    if (j.contains("Country")) {
        for (const auto& tag : j["Country"]) {
            Tag t;
            from_json(tag, t);
            item.countries.push_back(t);
        }
    }

    if (j.contains("Director")) {
        for (const auto& tag : j["Director"]) {
            Tag t;
            from_json(tag, t);
            item.directors.push_back(t);
        }
    }

    if (j.contains("Writer")) {
        for (const auto& tag : j["Writer"]) {
            Tag t;
            from_json(tag, t);
            item.writers.push_back(t);
        }
    }

    if (j.contains("Role")) {
        for (const auto& tag : j["Role"]) {
            Tag t;
            from_json(tag, t);
            item.cast.push_back(t);
        }
    }
}

void from_json(const nlohmann::json& j, Library& lib) {
    lib.key = safeGetInt(j, "key");
    lib.uuid = safeGetString(j, "uuid");
    lib.type = safeGetString(j, "type");
    lib.title = safeGetString(j, "title");
    lib.art = safeGetString(j, "art");
    lib.thumb = safeGetString(j, "thumb");
    lib.composite = safeGetString(j, "composite");
    lib.allowSync = safeGetBool(j, "allowSync");
    if (j.contains("updatedAt")) lib.updatedAt = j["updatedAt"].get<int64_t>();
    if (j.contains("scannedAt")) lib.scannedAt = j["scannedAt"].get<int64_t>();
}

void from_json(const nlohmann::json& j, Hub& hub) {
    hub.hubKey = safeGetString(j, "hubKey");
    hub.key = safeGetString(j, "key");
    hub.title = safeGetString(j, "title");
    hub.type = safeGetString(j, "type");
    hub.hubIdentifier = safeGetString(j, "hubIdentifier");
    hub.context = safeGetString(j, "context");
    hub.size = safeGetInt(j, "size");
    hub.more = safeGetBool(j, "more");
    hub.style = safeGetString(j, "style");
    hub.promoted = safeGetBool(j, "promoted");
    if (j.contains("Metadata")) {
        for (const auto& item : j["Metadata"]) {
            MediaItem m;
            from_json(item, m);
            hub.items.push_back(m);
        }
    }
    if (j.contains("Directory")) {
        MediaType dirType = MediaType::Person;
        std::string dirTypeStr = "person";

        if (hub.hubIdentifier.find("genre") != std::string::npos) {
            dirType = MediaType::Genre;
            dirTypeStr = "genre";
        } else if (hub.hubIdentifier.find("director") != std::string::npos) {
            dirType = MediaType::Director;
            dirTypeStr = "director";
        } else if (hub.hubIdentifier.find("writer") != std::string::npos) {
            dirType = MediaType::Writer;
            dirTypeStr = "writer";
        }

        for (const auto& dir : j["Directory"]) {
            MediaItem m;
            m.ratingKey = safeGetInt(dir, "id");
            m.title = safeGetString(dir, "tag");
            m.thumb = safeGetString(dir, "thumb");
            m.type = dirTypeStr;
            m.mediaType = dirType;
            m.librarySectionId = safeGetInt(dir, "librarySectionID");
            hub.items.push_back(m);
        }
    }
}

void from_json(const nlohmann::json& j, Collection& col) {
    col.ratingKey = safeGetInt(j, "ratingKey");
    col.key = safeGetString(j, "key");
    col.guid = safeGetString(j, "guid");
    col.type = safeGetString(j, "type");
    col.title = safeGetString(j, "title");
    col.summary = safeGetString(j, "summary");
    col.thumb = safeGetString(j, "thumb");
    col.art = safeGetString(j, "art");
    col.childCount = safeGetInt(j, "childCount");
    if (j.contains("addedAt")) col.addedAt = j["addedAt"].get<int64_t>();
    if (j.contains("updatedAt")) col.updatedAt = j["updatedAt"].get<int64_t>();
}

void from_json(const nlohmann::json& j, Playlist& pl) {
    pl.ratingKey = safeGetInt(j, "ratingKey");
    pl.key = safeGetString(j, "key");
    pl.guid = safeGetString(j, "guid");
    pl.type = safeGetString(j, "type");
    pl.title = safeGetString(j, "title");
    pl.summary = safeGetString(j, "summary");
    pl.thumb = safeGetString(j, "thumb");
    pl.composite = safeGetString(j, "composite");
    pl.leafCount = safeGetInt(j, "leafCount");
    if (j.contains("duration")) pl.duration = j["duration"].get<int64_t>();
    pl.smart = safeGetBool(j, "smart");
    pl.playlistType = safeGetString(j, "playlistType");
    if (j.contains("addedAt")) pl.addedAt = j["addedAt"].get<int64_t>();
    if (j.contains("updatedAt")) pl.updatedAt = j["updatedAt"].get<int64_t>();
}

}
