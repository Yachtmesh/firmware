#pragma once
#include <ArduinoJson.h>

#include "EnvironmentalSensorService.h"
#include "NMEA2000Service.h"
#include "Platform.h"
#include "Role.h"

struct WeatherStationConfig : public RoleConfig {
    uint8_t  inst       = 0;     // NMEA2000 environmental instance number
    uint32_t intervalMs = 2500;  // Broadcast interval (NMEA2000 recommends 2.5s for PGN 130311)

    WeatherStationConfig() = default;
    WeatherStationConfig(uint8_t i, uint32_t interval = 2500)
        : inst(i), intervalMs(interval) {}

    void toJson(JsonDocument& doc) const;
};

class WeatherStationRole : public Role {
   public:
    WeatherStationRole(EnvironmentalSensorInterface& sensor,
                       Nmea2000ServiceInterface& nmea,
                       PlatformInterface& platform);

    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    WeatherStationConfig config;  // public for test access

   private:
    EnvironmentalSensorInterface& sensor_;
    Nmea2000ServiceInterface&     nmea_;
    PlatformInterface&            platform_;
    uint32_t lastBroadcastMs_ = 0;
};
