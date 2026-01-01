#ifndef SAFFRON_PLEX_SERVER_HPP
#define SAFFRON_PLEX_SERVER_HPP

#include <functional>
#include <string>

class SettingsManager;

enum class ServerType {
    Local = 0,
    Remote = 1,
    Manual = 2
};

class PlexServer {
    friend class SettingsManager;
    friend class ServerDiscovery;

private:
    std::string m_machineId;
    std::string m_name;
    std::string m_address;
    int m_port = 32400;
    std::string m_accessToken;
    std::string m_version;
    bool m_owned = false;
    bool m_https = false;
    ServerType m_serverType = ServerType::Local;

    bool m_reachable = false;
    int64_t m_lastSeen = 0;

public:
    explicit PlexServer(const std::string& machineId);
    ~PlexServer();

    std::string getMachineId() const { return m_machineId; }
    std::string getName() const { return m_name; }
    std::string getAddress() const { return m_address; }
    int getPort() const { return m_port; }
    std::string getAccessToken() const { return m_accessToken; }
    std::string getVersion() const { return m_version; }
    bool isOwned() const { return m_owned; }
    bool isHttps() const { return m_https; }
    bool isReachable() const { return m_reachable; }
    ServerType getServerType() const { return m_serverType; }
    int64_t getLastSeen() const { return m_lastSeen; }

    void setName(const std::string& name) { m_name = name; }
    void setAddress(const std::string& address) { m_address = address; }
    void setPort(int port) { m_port = port; }
    void setAccessToken(const std::string& token) { m_accessToken = token; }
    void setVersion(const std::string& version) { m_version = version; }
    void setOwned(bool owned) { m_owned = owned; }
    void setHttps(bool https) { m_https = https; }
    void setReachable(bool reachable) { m_reachable = reachable; }
    void setServerType(ServerType type) { m_serverType = type; }
    void setLastSeen(int64_t timestamp) { m_lastSeen = timestamp; }

    std::string getBaseUrl() const;
    std::string getTranscodePictureUrl(const std::string& thumbPath, int width, int height) const;

    bool isLocal() const { return m_serverType == ServerType::Local; }
    bool isRemote() const { return m_serverType == ServerType::Remote; }
    bool isManual() const { return m_serverType == ServerType::Manual; }

    void testConnection(
        std::function<void()> onSuccess,
        std::function<void(const std::string&)> onError
    );
};

#endif
