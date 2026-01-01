#include "core/server_discovery.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"

#include <borealis.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <sstream>
#include <regex>

ServerDiscovery::ServerDiscovery(SettingsManager* settings)
    : m_settings(settings) {
}

ServerDiscovery::~ServerDiscovery() {
    stopDiscovery();
}

void ServerDiscovery::setGdmEnabled(bool enabled) {
    m_gdmEnabled = enabled;
}

void ServerDiscovery::stopDiscovery() {
    m_gdmRunning = false;

    if (m_gdmSocket >= 0) {
        close(m_gdmSocket);
        m_gdmSocket = -1;
    }
}

void ServerDiscovery::discoverLocalServersAsync(DiscoveryCompleteCallback onComplete) {
    if (m_gdmRunning) {
        return;
    }

    m_gdmRunning = true;

    brls::async([this, onComplete]() {
        doLocalDiscovery();

        brls::sync([this, onComplete]() {
            m_gdmRunning = false;
            if (onComplete) {
                onComplete();
            }
        });
    }, true);
}

void ServerDiscovery::doLocalDiscovery() {
    m_gdmSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_gdmSocket < 0) {
        return;
    }

    int broadcast = 1;
    setsockopt(m_gdmSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    int reuseaddr = 1;
    setsockopt(m_gdmSocket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(m_gdmSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = htons(0);

    if (bind(m_gdmSocket, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        close(m_gdmSocket);
        m_gdmSocket = -1;
        return;
    }

    sendGdmBroadcast();

    char buffer[4096];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);

    int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    while (m_gdmRunning) {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        if (now - startTime > GDM_TIMEOUT_MS) {
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t received = recvfrom(m_gdmSocket, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr*)&fromAddr, &fromLen);

        if (received > 0) {
            std::string fromAddrStr = inet_ntoa(fromAddr.sin_addr);

            GdmResponse response = parseGdmResponse(buffer, received, fromAddrStr);

            if (!response.machineId.empty()) {
                brls::sync([this, response]() {
                    PlexServer* server = m_settings->getOrCreateServer(response.machineId);
                    server->setName(response.name);
                    server->setAddress(response.address);
                    server->setPort(response.port);
                    server->setVersion(response.version);
                    server->setHttps(response.https);
                    server->setServerType(ServerType::Local);
                    server->setReachable(true);
                    server->setLastSeen(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count());

                    if (m_onServerFound) {
                        m_onServerFound(server);
                    }
                });
            }
        }
    }

    close(m_gdmSocket);
    m_gdmSocket = -1;
}

void ServerDiscovery::sendGdmBroadcast() {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(GDM_PORT);

    sendto(m_gdmSocket, GDM_MSG, strlen(GDM_MSG), 0,
           (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
}

GdmResponse ServerDiscovery::parseGdmResponse(const char* data, size_t len, const std::string& fromAddr) {
    GdmResponse response;
    response.address = fromAddr;

    std::string responseStr(data, len);
    std::istringstream stream(responseStr);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        while (!value.empty() && value.front() == ' ') {
            value.erase(0, 1);
        }

        if (key == "Name") {
            response.name = value;
        } else if (key == "Resource-Identifier") {
            response.machineId = value;
        } else if (key == "Port") {
            response.port = std::stoi(value);
        } else if (key == "Version") {
            response.version = value;
        }
    }

    return response;
}

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = reinterpret_cast<std::string*>(userdata);
    size_t count = size * nmemb;
    response->append(ptr, count);
    return count;
}

bool ServerDiscovery::doRemoteDiscovery(const std::string& plexToken, std::vector<PlexServer*>& outServers, std::string& outError) {
    if (plexToken.empty()) {
        outError = "No Plex token provided";
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
    headers = curl_slist_append(headers, ("X-Plex-Token: " + plexToken).c_str());
    headers = curl_slist_append(headers, ("X-Plex-Client-Identifier: " + m_settings->getClientId()).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://plex.tv/api/v2/resources?includeHttps=1&includeRelay=1");
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

        for (const auto& resource : json) {
            if (!resource.contains("provides") ||
                resource["provides"].get<std::string>().find("server") == std::string::npos) {
                continue;
            }

            std::string machineId = resource.value("clientIdentifier", "");
            if (machineId.empty()) continue;

            PlexServer* server = m_settings->getOrCreateServer(machineId);
            server->setName(resource.value("name", ""));
            server->setVersion(resource.value("productVersion", ""));
            server->setOwned(resource.value("owned", false));
            server->setServerType(ServerType::Remote);

            if (resource.contains("accessToken")) {
                server->setAccessToken(resource["accessToken"].get<std::string>());
            }

            if (resource.contains("connections")) {
                for (const auto& conn : resource["connections"]) {
                    bool local = conn.value("local", false);
                    if (!local) {
                        server->setAddress(conn.value("address", ""));
                        server->setPort(conn.value("port", 32400));
                        server->setHttps(conn.value("protocol", "http") == "https");
                        break;
                    }
                }

                if (server->getAddress().empty()) {
                    for (const auto& conn : resource["connections"]) {
                        server->setAddress(conn.value("address", ""));
                        server->setPort(conn.value("port", 32400));
                        server->setHttps(conn.value("protocol", "http") == "https");
                        break;
                    }
                }
            }

            outServers.push_back(server);
        }

        return true;

    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

void ServerDiscovery::discoverRemoteServersAsync(
    const std::string& plexToken,
    RemoteSuccessCallback onSuccess,
    ErrorCallback onError
) {
    brls::async([this, plexToken, onSuccess, onError]() {
        std::vector<PlexServer*> servers;
        std::string error;
        bool success = doRemoteDiscovery(plexToken, servers, error);

        brls::sync([success, servers, error, onSuccess, onError]() {
            if (success) {
                if (onSuccess) onSuccess(servers);
            } else {
                if (onError) onError(error);
            }
        });
    }, true);
}
