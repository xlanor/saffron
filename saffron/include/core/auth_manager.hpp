#ifndef SAFFRON_AUTH_MANAGER_HPP
#define SAFFRON_AUTH_MANAGER_HPP

#include <functional>
#include <string>

class SettingsManager;

struct PlexPin {
    int id = 0;
    std::string code;
    std::string clientIdentifier;
    int expiresIn = 0;
    bool trusted = false;
};

struct PlexUser {
    std::string username;
    std::string email;
    std::string thumb;
    std::string authToken;
};

enum class PinCheckResult {
    Waiting,      // User hasn't entered PIN yet
    Success,      // PIN claimed, auth complete
    Expired,      // PIN expired
    Error         // Network/other error
};

class AuthManager {
public:
    using OnPinReady = std::function<void(const PlexPin& pin)>;
    using OnAuthSuccess = std::function<void(const PlexUser& user)>;
    using OnError = std::function<void(const std::string& error)>;
    using OnExpired = std::function<void()>;

    explicit AuthManager(SettingsManager* settings);
    ~AuthManager();

private:
    SettingsManager* m_settings = nullptr;

    int m_currentPinId = 0;
    std::string m_currentPinCode;
    PlexPin m_currentPin;
    bool m_hasPendingPin = false;
    size_t m_pollTimerId = 0;

    // Stored callbacks for polling
    OnAuthSuccess m_onAuthSuccess;
    OnExpired m_onExpired;

    static constexpr const char* PLEX_TV_API = "https://plex.tv";
    static constexpr int PIN_POLL_INTERVAL_MS = 2000;

    // Internal sync HTTP methods (run on thread pool)
    bool doRequestPin(PlexPin& outPin, std::string& outError);
    PinCheckResult doCheckPinStatus(PlexUser& outUser, std::string& outError);
    bool doValidateToken(const std::string& token, PlexUser& outUser, std::string& outError);

public:
    // Async PIN request - runs HTTP on thread pool, callbacks on main thread
    void requestPinAsync(
        OnPinReady onSuccess,
        OnError onError
    );

    // Async PIN status check - runs HTTP on thread pool, callbacks on main thread
    void checkPinStatusAsync(
        OnAuthSuccess onSuccess,
        OnExpired onExpired,
        OnError onError
    );

    // Start polling for PIN claim every 2 seconds
    // Automatically stops when PIN is claimed or expired
    void startPinPolling(
        OnAuthSuccess onSuccess,
        OnExpired onExpired
    );

    // Stop polling if in progress
    void stopPinPolling();

    // Async token validation - runs HTTP on thread pool, callbacks on main thread
    void validateTokenAsync(
        const std::string& token,
        OnAuthSuccess onSuccess,
        OnError onError
    );

    void cancelPinAuth();
    void signOut();

    bool isAuthenticated() const;
    std::string getAuthToken() const;
    bool hasPendingPin() const { return m_hasPendingPin; }
};

#endif
