#pragma once

#include <cstring>
#include <string>

#include "WifiService.h"

// Mirrors WifiService ref-counting: multiple roles/managers can
// connect/disconnect without stomping on each other. Actual disconnect only
// happens at refCount 0.
class FakeWifiService : public WifiServiceInterface {
   public:
    bool connectCalled = false;
    bool disconnectCalled = false;
    bool connected = false;
    int refCount = 0;
    char lastSsid[33] = {0};
    char lastPassword[65] = {0};
    char ip[16] = {0};

    bool connect(const char* ssid, const char* password) override {
        connectCalled = true;
        refCount++;
        strncpy(lastSsid, ssid, sizeof(lastSsid) - 1);
        strncpy(lastPassword, password, sizeof(lastPassword) - 1);
        connected = true;
        return true;
    }

    void disconnect() override {
        disconnectCalled = true;
        if (refCount > 0) refCount--;
        if (refCount == 0) {
            connected = false;
            ip[0] = '\0';
        }
    }

    bool isConnected() const override { return connected; }
    std::string ipAddress() const override { return std::string(ip); }
};
