#pragma once

#include <ArduinoJson.h>

#include "NMEA2000Service.h"
#include "Role.h"
#include "TcpServer.h"
#include "WifiService.h"

struct WifiGatewayConfig : public RoleConfig {
    char ssid[33] = {0};
    char password[65] = {0};
    uint16_t port = 10110;

    void toJson(JsonDocument& doc) const override;
};

class WifiGatewayRole : public Role, public N2kListenerInterface {
   public:
    WifiGatewayRole(Nmea2000ServiceInterface& nmea,
                    WifiServiceInterface& wifi,
                    TcpServerInterface& tcpServer);

    // Role interface
    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    // N2kListenerInterface
    void onN2kData(const unsigned char* data, size_t len) override;

    WifiGatewayConfig config;

   private:
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
    TcpServerInterface& tcpServer_;
    bool tcpStarted_ = false;
};
