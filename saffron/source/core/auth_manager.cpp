#include "core/auth_manager.hpp"
#include "core/settings_manager.hpp"

#include <borealis.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

AuthManager::AuthManager(SettingsManager* settings)
    : m_settings(settings) {
}

AuthManager::~AuthManager() {
    stopPinPolling();
    cancelPinAuth();
}

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = reinterpret_cast<std::string*>(userdata);
    size_t count = size * nmemb;
    response->append(ptr, count);
    return count;
}

bool AuthManager::doRequestPin(PlexPin& outPin, std::string& outError) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "Failed to init curl";
        return false;
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    std::string clientId = m_settings->getClientId();
    std::string postData = "X-Plex-Product=Saffron";
    postData += "&X-Plex-Client-Identifier=" + clientId;
    postData += "&X-Plex-Platform=Nintendo Switch";
    postData += "&X-Plex-Device=Switch";

    curl_easy_setopt(curl, CURLOPT_URL, "https://plex.tv/api/v2/pins");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        outError = curl_easy_strerror(res);
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);

        outPin.id = json.value("id", 0);
        outPin.code = json.value("code", "");
        outPin.clientIdentifier = json.value("clientIdentifier", "");
        outPin.expiresIn = json.value("expiresIn", 0);
        outPin.trusted = json.value("trusted", false);

        return true;

    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

PinCheckResult AuthManager::doCheckPinStatus(PlexUser& outUser, std::string& outError) {
    if (!m_hasPendingPin || m_currentPinId == 0) {
        outError = "No pending PIN request";
        return PinCheckResult::Error;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "Failed to init curl";
        return PinCheckResult::Error;
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, ("X-Plex-Client-Identifier: " + m_settings->getClientId()).c_str());

    std::string url = "https://plex.tv/api/v2/pins/" + std::to_string(m_currentPinId);
    url += "?code=" + m_currentPinCode;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        outError = curl_easy_strerror(res);
        return PinCheckResult::Error;
    }

    try {
        auto json = nlohmann::json::parse(response);

        if (json.contains("authToken") && !json["authToken"].is_null()) {
            std::string authToken = json["authToken"].get<std::string>();

            if (!authToken.empty()) {
                m_settings->setPlexToken(authToken);
                m_settings->writeFile();

                outUser.authToken = authToken;
                m_hasPendingPin = false;
                m_currentPinId = 0;
                m_currentPinCode.clear();

                return PinCheckResult::Success;
            }
        }

        return PinCheckResult::Waiting;

    } catch (const std::exception& e) {
        outError = e.what();
        return PinCheckResult::Error;
    }
}

bool AuthManager::doValidateToken(const std::string& token, PlexUser& outUser, std::string& outError) {
    if (token.empty()) {
        outError = "No token provided";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "Failed to init curl";
        return false;
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, ("X-Plex-Token: " + token).c_str());
    headers = curl_slist_append(headers, ("X-Plex-Client-Identifier: " + m_settings->getClientId()).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://plex.tv/api/v2/user");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        outError = curl_easy_strerror(res);
        return false;
    }

    try {
        auto json = nlohmann::json::parse(response);

        outUser.username = json.value("username", "");
        outUser.email = json.value("email", "");
        outUser.thumb = json.value("thumb", "");
        outUser.authToken = token;

        m_settings->setUsername(outUser.username);
        m_settings->setPlexToken(token);
        m_settings->writeFile();

        return true;

    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

void AuthManager::requestPinAsync(OnPinReady onSuccess, OnError onError) {
    cancelPinAuth();

    brls::async([this, onSuccess, onError]() {
        PlexPin pin;
        std::string error;
        bool success = doRequestPin(pin, error);

        brls::sync([this, success, pin, error, onSuccess, onError]() {
            if (success) {
                m_currentPinId = pin.id;
                m_currentPinCode = pin.code;
                m_currentPin = pin;
                m_hasPendingPin = true;

                if (onSuccess) onSuccess(pin);
            } else {
                if (onError) onError(error);
            }
        });
    }, true);
}

void AuthManager::checkPinStatusAsync(OnAuthSuccess onSuccess, OnExpired onExpired, OnError onError) {
    brls::async([this, onSuccess, onExpired, onError]() {
        PlexUser user;
        std::string error;
        PinCheckResult result = doCheckPinStatus(user, error);

        brls::sync([result, user, error, onSuccess, onExpired, onError]() {
            switch (result) {
                case PinCheckResult::Success:
                    if (onSuccess) onSuccess(user);
                    break;
                case PinCheckResult::Waiting:
                    break;
                case PinCheckResult::Expired:
                    if (onExpired) onExpired();
                    break;
                case PinCheckResult::Error:
                    if (onExpired) onExpired();
                    break;
            }
        });
    }, true);
}

void AuthManager::startPinPolling(OnAuthSuccess onSuccess, OnExpired onExpired) {
    stopPinPolling();

    if (!m_hasPendingPin) {
        if (onExpired) onExpired();
        return;
    }

    m_onAuthSuccess = onSuccess;
    m_onExpired = onExpired;

    m_pollTimerId = brls::delay(PIN_POLL_INTERVAL_MS, [this]() {
        brls::async([this]() {
            PlexUser user;
            std::string error;
            PinCheckResult result = doCheckPinStatus(user, error);

            brls::sync([this, result, user]() {
                switch (result) {
                    case PinCheckResult::Success:
                        stopPinPolling();
                        if (m_onAuthSuccess) m_onAuthSuccess(user);
                        break;
                    case PinCheckResult::Waiting:
                        if (m_hasPendingPin) {
                            startPinPolling(m_onAuthSuccess, m_onExpired);
                        }
                        break;
                    case PinCheckResult::Expired:
                    case PinCheckResult::Error:
                        stopPinPolling();
                        if (m_onExpired) m_onExpired();
                        break;
                }
            });
        }, true);
    });
}

void AuthManager::stopPinPolling() {
    if (m_pollTimerId > 0) {
        brls::cancelDelay(m_pollTimerId);
        m_pollTimerId = 0;
    }
}

void AuthManager::validateTokenAsync(const std::string& token, OnAuthSuccess onSuccess, OnError onError) {
    brls::async([this, token, onSuccess, onError]() {
        PlexUser user;
        std::string error;
        bool success = doValidateToken(token, user, error);

        brls::sync([success, user, error, onSuccess, onError]() {
            if (success) {
                if (onSuccess) onSuccess(user);
            } else {
                if (onError) onError(error);
            }
        });
    }, true);
}

void AuthManager::cancelPinAuth() {
    stopPinPolling();
    m_currentPinId = 0;
    m_currentPinCode.clear();
    m_hasPendingPin = false;
    m_onAuthSuccess = nullptr;
    m_onExpired = nullptr;
}

void AuthManager::signOut() {
    cancelPinAuth();
    m_settings->clearAllServers();
    m_settings->clearTokenData();
    m_settings->writeFile();
}

bool AuthManager::isAuthenticated() const {
    return !m_settings->getPlexToken().empty();
}

std::string AuthManager::getAuthToken() const {
    return m_settings->getPlexToken();
}
