#include "core/settings_manager.hpp"
#include "core/auth_manager.hpp"
#include "core/plex_server.hpp"
#include "core/server_discovery.hpp"

#include <borealis.hpp>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <switch.h>

SettingsManager* SettingsManager::instance = nullptr;

SettingsManager::SettingsManager() {}

SettingsManager* SettingsManager::getInstance() {
    if (instance == nullptr) {
        instance = new SettingsManager();
        instance->ensureConfigDir();
        instance->parseFile();
    }
    return instance;
}

void SettingsManager::ensureConfigDir() {
    mkdir(CONFIG_DIR, 0755);
}

AuthManager* SettingsManager::getAuthManager() {
    if (!m_authManager) {
        m_authManager = std::make_unique<AuthManager>(this);
    }
    return m_authManager.get();
}

ServerDiscovery* SettingsManager::getServerDiscovery() {
    if (!m_serverDiscovery) {
        m_serverDiscovery = std::make_unique<ServerDiscovery>(this);
    }
    return m_serverDiscovery.get();
}

bool SettingsManager::fileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

std::string SettingsManager::generateUuid() {
    uint8_t bytes[16];
    csrngGetRandomBytes(bytes, sizeof(bytes));

    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 4; i++) ss << std::setw(2) << (int)bytes[i];
    ss << "-";
    for (int i = 4; i < 6; i++) ss << std::setw(2) << (int)bytes[i];
    ss << "-";
    for (int i = 6; i < 8; i++) ss << std::setw(2) << (int)bytes[i];
    ss << "-";
    for (int i = 8; i < 10; i++) ss << std::setw(2) << (int)bytes[i];
    ss << "-";
    for (int i = 10; i < 16; i++) ss << std::setw(2) << (int)bytes[i];

    return ss.str();
}

std::map<std::string, PlexServer*>* SettingsManager::getServersMap() {
    return &m_servers;
}

PlexServer* SettingsManager::getOrCreateServer(const std::string& machineId) {
    if (m_servers.find(machineId) == m_servers.end()) {
        m_servers[machineId] = new PlexServer(machineId);
    }
    return m_servers.at(machineId);
}

PlexServer* SettingsManager::findServerByAddress(const std::string& address) {
    for (auto& [id, server] : m_servers) {
        if (server && server->getAddress() == address) {
            return server;
        }
    }
    return nullptr;
}

void SettingsManager::removeServer(const std::string& machineId) {
    auto it = m_servers.find(machineId);
    if (it != m_servers.end()) {
        if (m_currentServerId == machineId) {
            m_currentServerId.clear();
        }
        delete it->second;
        m_servers.erase(it);
    }
}

void SettingsManager::clearAllServers() {
    for (auto& [id, server] : m_servers) {
        delete server;
    }
    m_servers.clear();
    m_currentServerId.clear();
}

PlexServer* SettingsManager::getCurrentServer() {
    if (m_currentServerId.empty()) {
        if (!m_servers.empty()) {
            m_currentServerId = m_servers.begin()->first;
        } else {
            return nullptr;
        }
    }
    auto it = m_servers.find(m_currentServerId);
    if (it != m_servers.end()) {
        return it->second;
    }
    return nullptr;
}

void SettingsManager::setCurrentServer(PlexServer* server) {
    if (server) {
        m_currentServerId = server->getMachineId();
    } else {
        m_currentServerId.clear();
    }
}

void SettingsManager::setCurrentServer(const std::string& machineId) {
    if (m_servers.find(machineId) != m_servers.end()) {
        m_currentServerId = machineId;
    }
}

void SettingsManager::parseFile() {
    if (fileExists(TOML_CONFIG_FILE)) {
        parseTomlFile();
    }
}

void SettingsManager::parseTomlFile() {
    try {
        auto config = toml::parse_file(TOML_CONFIG_FILE);

        if (auto val = config["plex_token"].value<std::string>())
            m_plexToken = *val;
        if (auto val = config["client_id"].value<std::string>())
            m_clientId = *val;
        if (auto val = config["username"].value<std::string>())
            m_username = *val;
        if (auto val = config["token_expires_at"].value<int64_t>())
            m_tokenExpiresAt = *val;

        if (auto val = config["auto_play_next"].value<bool>())
            m_autoPlayNext = *val;
        if (auto val = config["overclock"].value<bool>())
            m_overclockEnabled = *val;
        if (auto val = config["seek_increment"].value<int64_t>())
            m_seekIncrement = static_cast<int>(*val);
        if (auto val = config["in_memory_cache"].value<int64_t>())
            m_inMemoryCache = static_cast<int>(*val);
        if (auto val = config["power_user_menu_unlocked"].value<bool>())
            m_powerUserMenuUnlocked = *val;
        if (auto val = config["video_sync_mode"].value<std::string>())
            m_videoSyncMode = *val;
        if (auto val = config["framebuffer_count"].value<int64_t>())
            m_framebufferCount = static_cast<int>(*val);
        if (auto val = config["buffer_before_play"].value<bool>())
            m_bufferBeforePlay = *val;
        if (auto val = config["current_server_id"].value<std::string>())
            m_currentServerId = *val;

        for (auto& [key, value] : config) {
            if (!value.is_table()) continue;

            std::string keyStr(key.str());
            if (keyStr.rfind("server_", 0) != 0) continue;

            std::string machineId = keyStr.substr(7);
            auto* table = value.as_table();

            PlexServer* server = getOrCreateServer(machineId);

            if (auto val = (*table)["name"].value<std::string>())
                server->setName(*val);
            if (auto val = (*table)["address"].value<std::string>())
                server->setAddress(*val);
            if (auto val = (*table)["port"].value<int64_t>())
                server->setPort(static_cast<int>(*val));
            if (auto val = (*table)["access_token"].value<std::string>())
                server->setAccessToken(*val);
            if (auto val = (*table)["server_type"].value<int64_t>())
                server->setServerType(static_cast<ServerType>(*val));
            if (auto val = (*table)["https"].value<bool>())
                server->setHttps(*val);
        }

    } catch (const toml::parse_error& err) {
    }
}

int SettingsManager::writeFile() {
    ensureConfigDir();

    toml::table config;

    if (!m_plexToken.empty())
        config.insert("plex_token", m_plexToken);
    if (!m_clientId.empty())
        config.insert("client_id", m_clientId);
    if (!m_username.empty())
        config.insert("username", m_username);
    if (m_tokenExpiresAt > 0)
        config.insert("token_expires_at", m_tokenExpiresAt);

    config.insert("auto_play_next", m_autoPlayNext);
    config.insert("overclock", m_overclockEnabled);
    config.insert("seek_increment", m_seekIncrement);
    config.insert("in_memory_cache", m_inMemoryCache);
    if (m_powerUserMenuUnlocked)
        config.insert("power_user_menu_unlocked", m_powerUserMenuUnlocked);
    config.insert("video_sync_mode", m_videoSyncMode);
    config.insert("framebuffer_count", m_framebufferCount);
    config.insert("buffer_before_play", m_bufferBeforePlay);
    if (!m_currentServerId.empty())
        config.insert("current_server_id", m_currentServerId);

    for (const auto& [machineId, server] : m_servers) {
        toml::table serverTable;
        serverTable.insert("name", server->getName());
        serverTable.insert("address", server->getAddress());
        serverTable.insert("port", server->getPort());
        if (!server->getAccessToken().empty())
            serverTable.insert("access_token", server->getAccessToken());
        serverTable.insert("server_type", static_cast<int>(server->getServerType()));
        serverTable.insert("https", server->isHttps());

        config.insert("server_" + machineId, serverTable);
    }

    std::ofstream configFile(TOML_CONFIG_FILE, std::ios::out | std::ios::trunc);
    if (!configFile.is_open()) {
        return -1;
    }

    configFile << config;
    configFile.close();
    return 0;
}

std::string SettingsManager::getPlexToken() const {
    return m_plexToken;
}

void SettingsManager::setPlexToken(const std::string& token) {
    m_plexToken = token;
}

std::string SettingsManager::getClientId() {
    if (m_clientId.empty()) {
        generateClientId();
    }
    return m_clientId;
}

void SettingsManager::generateClientId() {
    m_clientId = generateUuid();
    writeFile();
}

std::string SettingsManager::getUsername() const {
    return m_username;
}

void SettingsManager::setUsername(const std::string& username) {
    m_username = username;
}

int64_t SettingsManager::getTokenExpiresAt() const {
    return m_tokenExpiresAt;
}

void SettingsManager::setTokenExpiresAt(int64_t expiresAt) {
    m_tokenExpiresAt = expiresAt;
}

void SettingsManager::clearTokenData() {
    m_plexToken.clear();
    m_username.clear();
    m_tokenExpiresAt = 0;
    brls::Logger::info("Plex token data cleared");
}

bool SettingsManager::isAutoPlayNextEnabled() const {
    return m_autoPlayNext;
}

void SettingsManager::setAutoPlayNext(bool enabled) {
    m_autoPlayNext = enabled;
}

bool SettingsManager::isOverclockEnabled() const {
    return m_overclockEnabled;
}

void SettingsManager::setOverclockEnabled(bool enabled) {
    m_overclockEnabled = enabled;
}

int SettingsManager::getSeekIncrement() const {
    return m_seekIncrement;
}

void SettingsManager::setSeekIncrement(int seconds) {
    m_seekIncrement = seconds;
}

int SettingsManager::getInMemoryCache() const {
    return m_inMemoryCache;
}

void SettingsManager::setInMemoryCache(int mb) {
    m_inMemoryCache = mb;
}

bool SettingsManager::isPowerUserMenuUnlocked() const {
    return m_powerUserMenuUnlocked;
}

void SettingsManager::setPowerUserMenuUnlocked(bool unlocked) {
    m_powerUserMenuUnlocked = unlocked;
}

std::string SettingsManager::getVideoSyncMode() const {
    return m_videoSyncMode;
}

void SettingsManager::setVideoSyncMode(const std::string& mode) {
    m_videoSyncMode = mode;
}

int SettingsManager::getFramebufferCount() const {
    return m_framebufferCount;
}

void SettingsManager::setFramebufferCount(int count) {
    if (count >= 2 && count <= 4) {
        m_framebufferCount = count;
    }
}

bool SettingsManager::isBufferBeforePlayEnabled() const {
    return m_bufferBeforePlay;
}

void SettingsManager::setBufferBeforePlay(bool enabled) {
    m_bufferBeforePlay = enabled;
}

int SettingsManager::loadEarlyFramebufferCount() {
    if (!fileExists(TOML_CONFIG_FILE)) {
        return 3;
    }
    try {
        auto config = toml::parse_file(TOML_CONFIG_FILE);
        if (auto val = config["framebuffer_count"].value<int64_t>()) {
            int count = static_cast<int>(*val);
            if (count >= 2 && count <= 4) {
                return count;
            }
        }
    } catch (...) {
    }
    return 3;
}

void SettingsManager::setOnServerChanged(std::function<void()> callback) {
    m_onServerChanged = callback;
}

void SettingsManager::triggerServerChanged() {
    if (m_onServerChanged) {
        m_onServerChanged();
    }
}
