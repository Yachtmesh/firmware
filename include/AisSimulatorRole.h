#pragma once

#include <ArduinoJson.h>

#include "AisEncoder.h"
#include "BoatSimulator.h"
#include "NMEA2000Service.h"
#include "Platform.h"
#include "Role.h"

struct AisSimulatorConfig : public RoleConfig {
    uint32_t intervalMs = 5000;

    void toJson(JsonDocument& doc) const override;
};

class AisSimulatorRole : public Role {
   public:
    AisSimulatorRole(Nmea2000ServiceInterface& nmea,
                     PlatformInterface& platform);

    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    AisSimulatorConfig config;

   private:
    Nmea2000ServiceInterface& nmea_;
    PlatformInterface& platform_;
    BoatSimulator simulator_;
    int nextBoat_ = 0;
    uint32_t lastEmitMs_ = 0;
    uint32_t lastUpdateMs_ = 0;
};
