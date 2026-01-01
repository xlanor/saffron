#include "core/plex_api.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"

#include <borealis.hpp>
#include <curl/curl.h>

#include <chrono>
#include <sstream>

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = reinterpret_cast<std::string*>(userdata);
    size_t count = size * nmemb;
    response->append(ptr, count);
    return count;
}

static std::string urlEncode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result = encoded ? encoded : value;
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::vector<std::string> PlexHeaders::toHeaderList() const {
    std::vector<std::string> headers;
    headers.push_back("Accept: application/json");
    headers.push_back("User-Agent: SHIELD Android TV");
    headers.push_back("X-Plex-Client-Identifier: " + clientIdentifier);
    headers.push_back("X-Plex-Product: " + product);
    headers.push_back("X-Plex-Version: " + version);
    headers.push_back("X-Plex-Platform: " + platform);
    headers.push_back("X-Plex-Device: " + device);
    headers.push_back("X-Plex-Model: " + model);
    if (!token.empty()) {
        headers.push_back("X-Plex-Token: " + token);
    }
    if (!sessionIdentifier.empty()) {
        headers.push_back("X-Plex-Session-Identifier: " + sessionIdentifier);
    }
    if (!clientProfileName.empty()) {
        headers.push_back("X-Plex-Client-Profile-Name: " + clientProfileName);
    }
    if (!clientProfileExtra.empty()) {
        headers.push_back("X-Plex-Client-Profile-Extra: " + clientProfileExtra);
    }
    return headers;
}

PlexHeaders PlexApi::buildHeaders() {
    PlexHeaders headers;
    headers.clientIdentifier = SettingsManager::getInstance()->getClientId();
    headers.token = SettingsManager::getInstance()->getPlexToken();
    return headers;
}

std::string PlexApi::buildUrl(PlexServer* server, const std::string& path) {
    std::string url = server->getBaseUrl();
    if (!path.empty() && path[0] != '/') {
        url += "/";
    }
    url += path;
    return url;
}

void PlexApi::get(
    const std::string& url,
    const PlexHeaders& headers,
    std::function<void(const nlohmann::json&)> onSuccess,
    OnError onError
) {
    brls::async([url, headers, onSuccess, onError]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            if (onError) brls::sync([onError]() { onError("Failed to init curl"); });
            return;
        }

        std::string response;
        struct curl_slist* headerList = nullptr;

        for (const auto& h : headers.toHeaderList()) {
            headerList = curl_slist_append(headerList, h.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
        curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::string error = curl_easy_strerror(res);
            if (onError) brls::sync([onError, error]() { onError(error); });
            return;
        }

        if (httpCode >= 400) {
            std::string error = "HTTP " + std::to_string(httpCode);
            brls::sync([error, response, url]() {
                brls::Logger::error("HTTP error {} for URL: {}", error, url);
                brls::Logger::error("Response body: {}", response.substr(0, 500));
            });
            if (onError) brls::sync([onError, error]() { onError(error); });
            return;
        }

        try {
            auto json = nlohmann::json::parse(response);
            brls::sync([onSuccess, json]() { onSuccess(json); });
        } catch (const std::exception& e) {
            std::string error = e.what();
            brls::sync([error, response, url, httpCode]() {
                brls::Logger::error("JSON parse error for URL: {} (HTTP {})", url, httpCode);
                brls::Logger::error("Response body: {}", response.substr(0, 500));
            });
            if (onError) brls::sync([onError, error]() { onError(error); });
        }
    });
}

void PlexApi::getLibrarySections(
    PlexServer* server,
    std::function<void(std::vector<plex::Library>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/sections");
    brls::Logger::info("PlexApi::getLibrarySections - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::Library> libraries;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Directory")) {
                for (const auto& dir : json["MediaContainer"]["Directory"]) {
                    plex::Library lib;
                    plex::from_json(dir, lib);
                    libraries.push_back(lib);
                }
            }
            if (onSuccess) onSuccess(libraries);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getLibraryItems(
    PlexServer* server,
    int sectionId,
    int start,
    int count,
    std::function<void(std::vector<plex::MediaItem>, int totalSize)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/sections/" + std::to_string(sectionId) + "/all");
    url += "?X-Plex-Container-Start=" + std::to_string(start);
    url += "&X-Plex-Container-Size=" + std::to_string(count);

    brls::Logger::info("PlexApi::getLibraryItems - sectionId={} start={} count={}", sectionId, start, count);
    brls::Logger::info("PlexApi::getLibraryItems - URL: {}", url);

    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError, sectionId](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            int totalSize = 0;

            if (json.contains("MediaContainer")) {
                auto& mc = json["MediaContainer"];
                if (mc.contains("totalSize")) {
                    totalSize = mc["totalSize"].get<int>();
                }
                brls::Logger::info("PlexApi::getLibraryItems - sectionId={} totalSize={}", sectionId, totalSize);

                if (mc.contains("Metadata")) {
                    for (const auto& meta : mc["Metadata"]) {
                        plex::MediaItem item;
                        plex::from_json(meta, item);
                        items.push_back(item);
                    }
                }
                brls::Logger::info("PlexApi::getLibraryItems - parsed {} items", items.size());
            } else {
                brls::Logger::warning("PlexApi::getLibraryItems - No MediaContainer in response");
            }
            if (onSuccess) onSuccess(items, totalSize);
        } catch (const std::exception& e) {
            brls::Logger::error("PlexApi::getLibraryItems - Parse error: {}", e.what());
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getRecentlyAdded(
    PlexServer* server,
    int sectionId,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/sections/" + std::to_string(sectionId) + "/recentlyAdded");
    brls::Logger::info("PlexApi::getRecentlyAdded - sectionId={} URL: {}", sectionId, url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getMetadata(
    PlexServer* server,
    int ratingKey,
    std::function<void(plex::MediaItem)> onSuccess,
    OnError onError
) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string url = buildUrl(server, "/library/metadata/" + std::to_string(ratingKey) + "?_=" + std::to_string(ms));
    brls::Logger::info("PlexApi::getMetadata - ratingKey={}", ratingKey);
    brls::Logger::info("PlexApi::getMetadata - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError, ratingKey](const nlohmann::json& json) {
        try {
            plex::MediaItem item;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                auto& metaArray = json["MediaContainer"]["Metadata"];
                if (!metaArray.empty()) {
                    plex::from_json(metaArray[0], item);
                    brls::Logger::info("PlexApi::getMetadata - parsed item '{}', cast={}, directors={}",
                        item.title, item.cast.size(), item.directors.size());
                }
            }
            if (onSuccess) onSuccess(item);
        } catch (const std::exception& e) {
            brls::Logger::error("PlexApi::getMetadata - error: {}", e.what());
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getChildren(
    PlexServer* server,
    int ratingKey,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/metadata/" + std::to_string(ratingKey) + "/children");
    brls::Logger::info("PlexApi::getChildren - ratingKey={} URL: {}", ratingKey, url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getHubs(
    PlexServer* server,
    std::function<void(std::vector<plex::Hub>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/hubs");
    brls::Logger::info("PlexApi::getHubs - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::Hub> hubs;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Hub")) {
                for (const auto& hubJson : json["MediaContainer"]["Hub"]) {
                    plex::Hub hub;
                    plex::from_json(hubJson, hub);
                    hubs.push_back(hub);
                }
            }
            if (onSuccess) onSuccess(hubs);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getContinueWatching(
    PlexServer* server,
    std::function<void(plex::Hub)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/hubs/continueWatching");
    brls::Logger::info("PlexApi::getContinueWatching - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            plex::Hub hub;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Hub")) {
                auto& hubArray = json["MediaContainer"]["Hub"];
                if (!hubArray.empty()) {
                    plex::from_json(hubArray[0], hub);
                }
            }
            if (onSuccess) onSuccess(hub);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getOnDeck(
    PlexServer* server,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/onDeck");
    brls::Logger::info("PlexApi::getOnDeck - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getCollections(
    PlexServer* server,
    int sectionId,
    std::function<void(std::vector<plex::Collection>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/sections/" + std::to_string(sectionId) + "/collections");
    brls::Logger::info("PlexApi::getCollections - sectionId={} URL: {}", sectionId, url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::Collection> collections;
            brls::Logger::debug("PlexApi::getCollections - response keys:");
            if (json.contains("MediaContainer")) {
                auto& mc = json["MediaContainer"];
                for (auto& [key, val] : mc.items()) {
                    brls::Logger::debug("  MediaContainer.{}", key);
                }
            }
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::Collection col;
                    plex::from_json(meta, col);
                    brls::Logger::debug("PlexApi::getCollections - found: {}", col.title);
                    collections.push_back(col);
                }
            }
            brls::Logger::info("PlexApi::getCollections - returning {} collections", collections.size());
            if (onSuccess) onSuccess(collections);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getCollectionItems(
    PlexServer* server,
    int collectionId,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/collections/" + std::to_string(collectionId) + "/children");
    brls::Logger::info("PlexApi::getCollectionItems - collectionId={} URL: {}", collectionId, url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getPlaylists(
    PlexServer* server,
    std::function<void(std::vector<plex::Playlist>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/playlists");
    brls::Logger::info("PlexApi::getPlaylists - URL: {}", url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::Playlist> playlists;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::Playlist pl;
                    plex::from_json(meta, pl);
                    playlists.push_back(pl);
                }
            }
            if (onSuccess) onSuccess(playlists);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getPlaylistItems(
    PlexServer* server,
    int playlistId,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/playlists/" + std::to_string(playlistId) + "/items");
    brls::Logger::info("PlexApi::getPlaylistItems - playlistId={} URL: {}", playlistId, url);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getPlaybackDecision(
    PlexServer* server,
    int ratingKey,
    int maxBitrate,
    const std::string& resolution,
    bool forceTranscode,
    int64_t offsetMs,
    int mediaIndex,
    std::function<void(plex::PlaybackInfo)> onSuccess,
    OnError onError
) {
    std::string sessionId = SettingsManager::getInstance()->generateUuid();

    std::stringstream ss;
    ss << "/video/:/transcode/universal/decision";
    ss << "?path=" << urlEncode("/library/metadata/" + std::to_string(ratingKey));
    ss << "&mediaIndex=" << mediaIndex;
    ss << "&partIndex=0";
    ss << "&protocol=hls";
    if (forceTranscode) {
        ss << "&directPlay=0";
        ss << "&directStream=0";
    } else {
        ss << "&directPlay=1";
        ss << "&directStream=1";
    }
    ss << "&directStreamAudio=1";
    ss << "&autoAdjustQuality=1";
    ss << "&location=lan";
    ss << "&mediaBufferSize=102400";
    ss << "&transcodeSessionId=" << sessionId;
    if (forceTranscode) {
        ss << "&subtitles=auto";
        ss << "&advancedSubtitles=burn";
        ss << "&subtitleSize=100";
    } else {
        ss << "&subtitles=none";
    }
    ss << "&audioBoost=100";

    if (maxBitrate > 0) {
        ss << "&videoBitrate=" << maxBitrate;
    }
    if (!resolution.empty()) {
        ss << "&videoResolution=" << resolution;
    }

    std::string url = buildUrl(server, ss.str());
    brls::Logger::info("PlexApi::getPlaybackDecision URL: {}", url);

    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }
    headers.sessionIdentifier = sessionId;

    std::string baseUrl = server->getBaseUrl();
    std::string token = server->getAccessToken();
    std::string requestUrl = ss.str();
    std::string clientId = SettingsManager::getInstance()->getClientId();

    int64_t offsetSec = offsetMs / 1000;
    get(url, headers, [onSuccess, onError, baseUrl, token, forceTranscode, maxBitrate, resolution, offsetSec, ratingKey, sessionId, mediaIndex](const nlohmann::json& json) {
        try {
            brls::Logger::info("Decision response: {}", json.dump());
            plex::PlaybackInfo info;
            if (json.contains("MediaContainer")) {
                auto& mc = json["MediaContainer"];
                if (mc.contains("generalDecisionText"))
                    info.generalDecisionText = mc["generalDecisionText"].get<std::string>();
                if (mc.contains("generalDecisionCode"))
                    info.generalDecisionCode = mc["generalDecisionCode"].get<int>();
                if (mc.contains("directPlayDecisionText"))
                    info.directPlayDecisionText = mc["directPlayDecisionText"].get<std::string>();
                if (mc.contains("directPlayDecisionCode"))
                    info.directPlayDecisionCode = mc["directPlayDecisionCode"].get<int>();
                if (mc.contains("transcodeDecisionText"))
                    info.transcodeDecisionText = mc["transcodeDecisionText"].get<std::string>();
                if (mc.contains("transcodeDecisionCode"))
                    info.transcodeDecisionCode = mc["transcodeDecisionCode"].get<int>();

                info.directPlayable = (info.directPlayDecisionCode >= 1000 && info.directPlayDecisionCode < 2000);

                if (mc.contains("Metadata") && !mc["Metadata"].empty()) {
                    plex::from_json(mc["Metadata"][0], info.metadata);
                }

                bool hasDirectUrl = !info.metadata.media.empty() &&
                    !info.metadata.media[0].parts.empty() &&
                    !info.metadata.media[0].parts[0].key.empty();

                if (!forceTranscode && hasDirectUrl) {
                    std::string partKey = info.metadata.media[0].parts[0].key;
                    info.playbackUrl = baseUrl + partKey + "?X-Plex-Token=" + token;
                    info.protocol = "http";
                    brls::Logger::info("Direct play URL: {}", info.playbackUrl);
                } else {
                    std::stringstream startUrl;
                    startUrl << "/video/:/transcode/universal/start.m3u8";
                    startUrl << "?path=" << urlEncode("/library/metadata/" + std::to_string(ratingKey));
                    startUrl << "&mediaIndex=" << mediaIndex << "&partIndex=0";
                    startUrl << "&fastSeek=1&copyts=1&offset=" << offsetSec;
                    startUrl << "&X-Plex-Platform=Chrome";
                    startUrl << "&X-Plex-Token=" << token;
                    startUrl << "&session=" << sessionId;
                    startUrl << "&directStream=0";
                    startUrl << "&protocol=hls";
                    if (maxBitrate > 0) {
                        startUrl << "&maxVideoBitrate=" << maxBitrate;
                    }
                    if (!resolution.empty()) {
                        std::string videoRes;
                        if (resolution == "1080p") videoRes = "1920x1080";
                        else if (resolution == "720p") videoRes = "1280x720";
                        else if (resolution == "480p") videoRes = "854x480";
                        else if (resolution == "360p") videoRes = "640x360";
                        else if (resolution == "320p") videoRes = "480x320";
                        else videoRes = resolution;
                        startUrl << "&videoResolution=" << videoRes;
                    }
                    info.playbackUrl = baseUrl + startUrl.str();
                    info.protocol = "hls";
                    info.sessionId = sessionId;
                    brls::Logger::info("HLS transcode URL: {}", info.playbackUrl);
                }
            }
            if (onSuccess) onSuccess(info);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::reportTimeline(
    PlexServer* server,
    int ratingKey,
    int64_t time,
    const std::string& state,
    int64_t duration,
    const std::string& sessionId,
    OnError onError
) {
    std::stringstream ss;
    ss << "/:/timeline";
    ss << "?ratingKey=" << ratingKey;
    ss << "&key=/library/metadata/" << ratingKey;
    ss << "&time=" << time;
    ss << "&duration=" << duration;
    ss << "&state=" << state;
    ss << "&playbackTime=" << time;

    std::string url = buildUrl(server, ss.str());
    brls::Logger::info("PlexApi::reportTimeline - ratingKey={} time={} state={} sessionId={}", ratingKey, time, state, sessionId);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }
    if (!sessionId.empty()) {
        headers.sessionIdentifier = sessionId;
    }

    brls::async([url, headers, onError]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            if (onError) brls::sync([onError]() { onError("Failed to init curl"); });
            return;
        }

        std::string response;
        struct curl_slist* headerList = nullptr;

        for (const auto& h : headers.toHeaderList()) {
            headerList = curl_slist_append(headerList, h.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::string error = curl_easy_strerror(res);
            brls::sync([error]() {
                brls::Logger::error("Timeline report failed: {}", error);
            });
            if (onError) brls::sync([onError, error]() { onError(error); });
            return;
        }

        if (httpCode >= 400) {
            brls::sync([httpCode]() {
                brls::Logger::error("Timeline report HTTP error: {}", httpCode);
            });
        } else {
            brls::sync([httpCode]() {
                brls::Logger::debug("Timeline reported successfully (HTTP {})", httpCode);
            });
        }
    });
}

void PlexApi::search(
    PlexServer* server,
    const std::string& query,
    int limit,
    std::function<void(std::vector<plex::Hub>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/hubs/search?query=" + urlEncode(query) + "&limit=" + std::to_string(limit));
    brls::Logger::info("PlexApi::search - query='{}' limit={}", query, limit);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::Hub> hubs;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Hub")) {
                for (const auto& hubJson : json["MediaContainer"]["Hub"]) {
                    plex::Hub hub;
                    plex::from_json(hubJson, hub);
                    hubs.push_back(hub);
                }
            }
            if (onSuccess) onSuccess(hubs);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getPersonMedia(
    PlexServer* server,
    int personId,
    std::function<void(std::vector<plex::MediaItem>)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, "/library/people/" + std::to_string(personId) + "/media");
    brls::Logger::info("PlexApi::getPersonMedia - personId={}", personId);
    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            if (json.contains("MediaContainer") && json["MediaContainer"].contains("Metadata")) {
                for (const auto& meta : json["MediaContainer"]["Metadata"]) {
                    plex::MediaItem item;
                    plex::from_json(meta, item);
                    items.push_back(item);
                }
            }
            if (onSuccess) onSuccess(items);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}

void PlexApi::getTagMedia(
    PlexServer* server,
    const std::string& endpoint,
    int start,
    int count,
    std::function<void(std::vector<plex::MediaItem>, int totalSize)> onSuccess,
    OnError onError
) {
    std::string url = buildUrl(server, endpoint);
    // Add pagination parameters
    url += (endpoint.find('?') != std::string::npos ? "&" : "?");
    url += "X-Plex-Container-Start=" + std::to_string(start);
    url += "&X-Plex-Container-Size=" + std::to_string(count);

    brls::Logger::info("PlexApi::getTagMedia - endpoint={} start={} count={}", endpoint, start, count);

    PlexHeaders headers = buildHeaders();
    if (!server->getAccessToken().empty()) {
        headers.token = server->getAccessToken();
    }

    get(url, headers, [onSuccess, onError](const nlohmann::json& json) {
        try {
            std::vector<plex::MediaItem> items;
            int totalSize = 0;

            if (json.contains("MediaContainer")) {
                auto& mc = json["MediaContainer"];
                if (mc.contains("totalSize")) {
                    totalSize = mc["totalSize"].get<int>();
                } else if (mc.contains("size")) {
                    totalSize = mc["size"].get<int>();
                }

                if (mc.contains("Metadata")) {
                    for (const auto& meta : mc["Metadata"]) {
                        plex::MediaItem item;
                        plex::from_json(meta, item);
                        items.push_back(item);
                    }
                }
            }
            if (onSuccess) onSuccess(items, totalSize);
        } catch (const std::exception& e) {
            if (onError) onError(e.what());
        }
    }, onError);
}
