#ifndef SAFFRON_SERVER_LIST_TAB_HPP
#define SAFFRON_SERVER_LIST_TAB_HPP

#include <borealis.hpp>
#include <unordered_map>
#include <memory>

class PlexServer;
class SettingsManager;
class ServerDiscovery;
class AuthManager;
class ServerItemView;

class ServerListTab : public brls::Box {
    friend class ServerItemView;

public:
    ServerListTab();
    ~ServerListTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    void updateServerItem(PlexServer* server);
    void syncServerList();
    void refreshServers();

private:
    BRLS_BIND(brls::Box, serverContainer, "server/container");
    BRLS_BIND(brls::Box, emptyMessage, "empty/message");
    BRLS_BIND(brls::Button, refreshBtn, "server/refreshBtn");
    BRLS_BIND(brls::Button, loginBtn, "server/loginBtn");
    BRLS_BIND(brls::Label, accountLabel, "server/accountLabel");

    void initButtons();
    void updateAccountStatus();
    void startPinLogin();

    SettingsManager* m_settings = nullptr;
    ServerDiscovery* m_discovery = nullptr;
    AuthManager* m_auth = nullptr;
    brls::Dialog* m_pinDialog = nullptr;

    std::unordered_map<PlexServer*, ServerItemView*> m_serverItems;

    static ServerListTab* s_currentInstance;
    static bool s_isActive;
};

class ServerItemView : public brls::Box {
public:
    ServerItemView(PlexServer* server);

    void updateFromServer();
    PlexServer* getServer() const { return m_server; }

private:
    PlexServer* m_server = nullptr;

    brls::Label* m_nameLabel = nullptr;
    brls::Label* m_addressLabel = nullptr;
    brls::Label* m_statusLabel = nullptr;
};

#endif
