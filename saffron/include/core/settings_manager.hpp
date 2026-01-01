#ifndef SAFFRON_SETTINGS_MANAGER_HPP
#define SAFFRON_SETTINGS_MANAGER_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class AuthManager;
class PlexServer;
class ServerDiscovery;

class SettingsManager {
protected:
    SettingsManager();
    static SettingsManager* instance;

private:
    static constexpr const char* CONFIG_DIR = "sdmc:/switch/saffron";
    static constexpr const char* TOML_CONFIG_FILE = "sdmc:/switch/saffron/config.toml";

    std::map<std::string, PlexServer*> m_servers;
    std::string m_currentServerId;
    std::unique_ptr<AuthManager> m_authManager;
    std::unique_ptr<ServerDiscovery> m_serverDiscovery;

    std::string m_plexToken;
    std::string m_clientId;
    std::string m_username;
    int64_t m_tokenExpiresAt = 0;

    bool m_autoPlayNext = true;
    bool m_overclockEnabled = false;
    int m_seekIncrement = 10;
    int m_inMemoryCache = 50;
    bool m_powerUserMenuUnlocked = false;
    std::string m_videoSyncMode = "audio";
    int m_framebufferCount = 3;
    bool m_bufferBeforePlay = false;

    std::function<void()> m_onServerChanged;

    void parseTomlFile();
    static bool fileExists(const char* path);

public:
    static std::string generateUuid();
    SettingsManager(const SettingsManager&) = delete;
    void operator=(const SettingsManager&) = delete;
    static SettingsManager* getInstance();

    void parseFile();
    int writeFile();
    void ensureConfigDir();

    AuthManager* getAuthManager();
    ServerDiscovery* getServerDiscovery();

    std::map<std::string, PlexServer*>* getServersMap();
    PlexServer* getOrCreateServer(const std::string& machineId);
    PlexServer* findServerByAddress(const std::string& address);
    void removeServer(const std::string& machineId);

    PlexServer* getCurrentServer();
    void setCurrentServer(PlexServer* server);
    void setCurrentServer(const std::string& machineId);
    void clearAllServers();

    std::string getPlexToken() const;
    void setPlexToken(const std::string& token);

    std::string getClientId();
    void generateClientId();

    std::string getUsername() const;
    void setUsername(const std::string& username);

    int64_t getTokenExpiresAt() const;
    void setTokenExpiresAt(int64_t expiresAt);
    void clearTokenData();

    bool isAutoPlayNextEnabled() const;
    void setAutoPlayNext(bool enabled);

    bool isOverclockEnabled() const;
    void setOverclockEnabled(bool enabled);

    int getSeekIncrement() const;
    void setSeekIncrement(int seconds);

    int getInMemoryCache() const;
    void setInMemoryCache(int mb);

    bool isPowerUserMenuUnlocked() const;
    void setPowerUserMenuUnlocked(bool unlocked);

    std::string getVideoSyncMode() const;
    void setVideoSyncMode(const std::string& mode);

    int getFramebufferCount() const;
    void setFramebufferCount(int count);

    bool isBufferBeforePlayEnabled() const;
    void setBufferBeforePlay(bool enabled);

    static int loadEarlyFramebufferCount();

    void setOnServerChanged(std::function<void()> callback);
    void triggerServerChanged();
};

#endif
