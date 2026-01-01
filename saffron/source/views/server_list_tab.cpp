#include "views/server_list_tab.hpp"
#include "core/plex_server.hpp"
#include "core/settings_manager.hpp"
#include "core/server_discovery.hpp"
#include "core/auth_manager.hpp"

ServerListTab* ServerListTab::s_currentInstance = nullptr;
bool ServerListTab::s_isActive = false;

ServerListTab::ServerListTab() {
    s_currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/server_list.xml");

    m_settings = SettingsManager::getInstance();
    m_discovery = m_settings->getServerDiscovery();
    m_auth = m_settings->getAuthManager();

    m_discovery->setOnServerFound([](PlexServer* server) {
        if (!s_isActive || !s_currentInstance) return;
        s_currentInstance->updateServerItem(server);
    });

    initButtons();
    updateAccountStatus();
    syncServerList();
}

ServerListTab::~ServerListTab() {
    if (s_currentInstance == this) {
        s_currentInstance = nullptr;
    }
    m_serverItems.clear();
    m_discovery->setOnServerFound(nullptr);
    m_auth->stopPinPolling();
    if (m_pinDialog) {
        m_pinDialog->dismiss();
        m_pinDialog = nullptr;
    }
}

brls::View* ServerListTab::create() {
    return new ServerListTab();
}

void ServerListTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    s_isActive = true;
    brls::Logger::debug("ServerListTab::willAppear - resuming discovery callbacks");
    syncServerList();

    if (m_auth->isAuthenticated()) {
        refreshServers();
    }
}

void ServerListTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    s_isActive = false;
    brls::Logger::debug("ServerListTab::willDisappear - pausing discovery callbacks");
}

void ServerListTab::initButtons() {
    if (refreshBtn) {
        refreshBtn->registerClickAction([this](brls::View*) {
            refreshServers();
            return true;
        });
    }

    if (loginBtn) {
        loginBtn->registerClickAction([this](brls::View*) {
            if (m_auth->isAuthenticated()) {
                brls::Dialog* dialog = new brls::Dialog("Sign out of Plex?");
                dialog->addButton("Cancel", []() {});
                dialog->addButton("Sign Out", [this]() {
                    m_auth->signOut();
                    updateAccountStatus();
                    syncServerList();
                });
                dialog->open();
            } else {
                startPinLogin();
            }
            return true;
        });
    }
}

void ServerListTab::startPinLogin() {
    m_auth->requestPinAsync(
        [this](const PlexPin& pin) {
            auto* contentBox = new brls::Box();
            contentBox->setAxis(brls::Axis::COLUMN);
            contentBox->setAlignItems(brls::AlignItems::CENTER);
            contentBox->setPadding(20);

            auto* urlLabel = new brls::Label();
            urlLabel->setText("Go to plex.tv/link and enter:");
            urlLabel->setFontSize(18);
            urlLabel->setMarginBottom(20);
            contentBox->addView(urlLabel);

            auto* codeLabel = new brls::Label();
            codeLabel->setText(pin.code);
            codeLabel->setFontSize(48);
            codeLabel->setTextColor(nvgRGBA(229, 160, 13, 255));
            contentBox->addView(codeLabel);

            m_pinDialog = new brls::Dialog(contentBox);
            m_pinDialog->addButton("OK", [this]() {
                m_pinDialog = nullptr;
                updateAccountStatus();
            });
            m_pinDialog->open();

            m_auth->startPinPolling(
                [this](const PlexUser& user) {
                    if (m_pinDialog) {
                        m_pinDialog->dismiss([this, user]() {
                            m_pinDialog = nullptr;
                            brls::Application::notify("Signed in as " + user.username);
                            updateAccountStatus();
                            refreshServers();
                            m_settings->triggerServerChanged();
                        });
                    } else {
                        brls::Application::notify("Signed in as " + user.username);
                        updateAccountStatus();
                        refreshServers();
                        m_settings->triggerServerChanged();
                    }
                },
                [this]() {
                    if (m_pinDialog) {
                        m_pinDialog->dismiss([this]() {
                            m_pinDialog = nullptr;
                            brls::Application::notify("PIN expired");
                            updateAccountStatus();
                        });
                    } else {
                        brls::Application::notify("PIN expired");
                        updateAccountStatus();
                    }
                }
            );
        },
        [](const std::string& error) {
            brls::Application::notify("Failed to get PIN: " + error);
        }
    );
}

void ServerListTab::updateAccountStatus() {
    if (accountLabel) {
        if (m_auth->isAuthenticated()) {
            std::string username = m_settings->getUsername();
            if (username.empty()) {
                accountLabel->setText("Signed in");
            } else {
                accountLabel->setText("Signed in as " + username);
            }
        } else if (m_auth->hasPendingPin()) {
            accountLabel->setText("PIN requested - complete at plex.tv/link");
        } else {
            accountLabel->setText("Not signed in");
        }
    }

    if (loginBtn) {
        if (m_auth->isAuthenticated()) {
            loginBtn->setText("Sign Out");
        } else if (m_auth->hasPendingPin()) {
            loginBtn->setText("Waiting...");
        } else {
            loginBtn->setText("Sign In");
        }
    }
}

void ServerListTab::syncServerList() {
    if (!serverContainer) return;

    brls::Application::giveFocus(this);

    serverContainer->clearViews();
    m_serverItems.clear();

    auto* serversMap = m_settings->getServersMap();

    for (const auto& [machineId, server] : *serversMap) {
        updateServerItem(server);
    }

    if (emptyMessage) {
        emptyMessage->setVisibility(m_serverItems.empty() ?
            brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }

    if (!m_serverItems.empty() && serverContainer) {
        brls::Application::giveFocus(serverContainer);
    } else if (refreshBtn) {
        brls::Application::giveFocus(refreshBtn);
    }
}

void ServerListTab::updateServerItem(PlexServer* server) {
    if (!serverContainer) return;

    auto it = m_serverItems.find(server);
    if (it != m_serverItems.end()) {
        it->second->updateFromServer();
    } else {
        ServerItemView* item = new ServerItemView(server);
        serverContainer->addView(item);
        m_serverItems[server] = item;

        if (emptyMessage) {
            emptyMessage->setVisibility(brls::Visibility::GONE);
        }
    }
}

void ServerListTab::refreshServers() {
    brls::Logger::info("Refreshing server list");

    m_discovery->discoverLocalServersAsync([]() {
        brls::Logger::info("Local discovery complete");
    });

    if (m_auth->isAuthenticated()) {
        m_discovery->discoverRemoteServersAsync(
            m_auth->getAuthToken(),
            [this](std::vector<PlexServer*> servers) {
                brls::Logger::info("Found {} remote servers", servers.size());
                m_settings->writeFile();
                brls::Logger::info("Saved server config to disk");
                if (s_isActive && s_currentInstance) {
                    s_currentInstance->syncServerList();
                }
            },
            [](const std::string& error) {
                brls::Logger::error("Remote discovery failed: {}", error);
            }
        );
    }

    syncServerList();
}

ServerItemView::ServerItemView(PlexServer* server)
    : m_server(server) {

    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(15);
    this->setMarginBottom(10);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(8);
    this->setFocusable(true);

    m_nameLabel = new brls::Label();
    m_nameLabel->setFontSize(20);
    m_nameLabel->setMarginBottom(5);
    this->addView(m_nameLabel);

    m_addressLabel = new brls::Label();
    m_addressLabel->setFontSize(16);
    m_addressLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_addressLabel->setMarginBottom(5);
    this->addView(m_addressLabel);

    m_statusLabel = new brls::Label();
    m_statusLabel->setFontSize(14);
    this->addView(m_statusLabel);

    updateFromServer();

    this->registerClickAction([this](brls::View*) {
        SettingsManager::getInstance()->setCurrentServer(m_server);
        SettingsManager::getInstance()->writeFile();
        brls::Application::notify("Connected to " + m_server->getName());
        SettingsManager::getInstance()->triggerServerChanged();
        return true;
    });
}

void ServerItemView::updateFromServer() {
    if (m_nameLabel) {
        m_nameLabel->setText(m_server->getName());
    }

    if (m_addressLabel) {
        std::string addr = m_server->getAddress();
        if (m_server->getPort() != 32400) {
            addr += ":" + std::to_string(m_server->getPort());
        }
        m_addressLabel->setText(addr);
    }

    if (m_statusLabel) {
        std::string status;
        if (m_server->isReachable()) {
            status = "Online";
            m_statusLabel->setTextColor(nvgRGBA(100, 200, 100, 255));
        } else {
            status = "Offline";
            m_statusLabel->setTextColor(nvgRGBA(200, 100, 100, 255));
        }

        if (m_server->isLocal()) {
            status += " (Local)";
        } else if (m_server->isRemote()) {
            status += " (Remote)";
        }

        m_statusLabel->setText(status);
    }
}
