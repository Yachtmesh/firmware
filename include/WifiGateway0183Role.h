#pragma once

#include <ArduinoJson.h>

#include "AisN2kTo0183Converter.h"
#include "NMEA2000Service.h"
#include "Role.h"
#include "TcpServer.h"
#include "WifiService.h"

struct WifiGateway0183Config : public RoleConfig {
    char ssid[33] = {0};
    char password[65] = {0};
    uint16_t port = 10110;

    void toJson(JsonDocument& doc) const override;
};

class WifiGateway0183Role : public Role, public N2kListenerInterface {
   public:
    WifiGateway0183Role(Nmea2000ServiceInterface& nmea,
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
    void onN2kMessage(uint32_t pgn, uint8_t priority, uint8_t source,
                      const unsigned char* data, size_t len) override;

    WifiGateway0183Config config;

   private:
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
    TcpServerInterface& tcpServer_;
    AisN2kTo0183Converter converter_;
    bool tcpStarted_ = false;
};
