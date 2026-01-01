#ifndef SAFFRON_SERVER_DISCOVERY_HPP
#define SAFFRON_SERVER_DISCOVERY_HPP

#include <functional>
#include <string>
#include <atomic>
#include <vector>

class PlexServer;
class SettingsManager;

struct GdmResponse {
    std::string name;
    std::string machineId;
    std::string address;
    int port = 32400;
    std::string version;
    bool https = false;
};

class ServerDiscovery {
public:
    using ServerFoundCallback = std::function<void(PlexServer*)>;
    using DiscoveryCompleteCallback = std::function<void()>;
    using RemoteSuccessCallback = std::function<void(std::vector<PlexServer*>)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    explicit ServerDiscovery(SettingsManager* settings);
    ~ServerDiscovery();

private:
    SettingsManager* m_settings = nullptr;
    ServerFoundCallback m_onServerFound;

    std::atomic<bool> m_gdmEnabled{false};
    std::atomic<bool> m_gdmRunning{false};
    int m_gdmSocket = -1;

    static constexpr int GDM_PORT = 32414;
    static constexpr const char* GDM_MSG = "M-SEARCH * HTTP/1.1\r\n\r\n";
    static constexpr int GDM_TIMEOUT_MS = 3000;

    void doLocalDiscovery();
    void sendGdmBroadcast();
    GdmResponse parseGdmResponse(const char* data, size_t len, const std::string& fromAddr);
    bool doRemoteDiscovery(const std::string& plexToken, std::vector<PlexServer*>& outServers, std::string& outError);

public:
    void setGdmEnabled(bool enabled);
    bool isGdmEnabled() const { return m_gdmEnabled; }

    void discoverLocalServersAsync(DiscoveryCompleteCallback onComplete = nullptr);
    void stopDiscovery();

    void discoverRemoteServersAsync(
        const std::string& plexToken,
        RemoteSuccessCallback onSuccess,
        ErrorCallback onError
    );

    void setOnServerFound(ServerFoundCallback callback) {
        m_onServerFound = std::move(callback);
    }
};

#endif
