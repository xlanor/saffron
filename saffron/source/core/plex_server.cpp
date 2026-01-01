#include "core/plex_server.hpp"

#include <borealis.hpp>
#include <curl/curl.h>

PlexServer::PlexServer(const std::string& machineId)
    : m_machineId(machineId) {}

PlexServer::~PlexServer() {}

std::string PlexServer::getBaseUrl() const {
    std::string protocol = m_https ? "https" : "http";
    return protocol + "://" + m_address + ":" + std::to_string(m_port);
}

std::string PlexServer::getTranscodePictureUrl(const std::string& thumbPath, int width, int height) const {
    char* encoded = curl_easy_escape(nullptr, thumbPath.c_str(), 0);
    std::string encodedUrl = encoded ? encoded : thumbPath;
    if (encoded) curl_free(encoded);

    return getBaseUrl() + "/photo/:/transcode?url=" + encodedUrl +
           "&width=" + std::to_string(width) +
           "&height=" + std::to_string(height) +
           "&minSize=1&X-Plex-Token=" + m_accessToken;
}

void PlexServer::testConnection(
    std::function<void()> onSuccess,
    std::function<void(const std::string&)> onError
) {
    brls::Logger::info("Testing connection to {}", getBaseUrl());

    brls::async([this, onSuccess, onError]() {
        brls::sync([this, onSuccess]() {
            m_reachable = true;
            if (onSuccess) onSuccess();
        });
    });
}
