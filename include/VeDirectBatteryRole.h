#pragma once

#include <ArduinoJson.h>

#include "NMEA2000Service.h"
#include "Role.h"
#include "SerialSensorService.h"
#include "VeDirectParser.h"

struct VeDirectBatteryConfig : public RoleConfig {
    uint8_t inst = 0;

    void toJson(JsonDocument& doc) const;
};

class VeDirectBatteryRole : public Role {
   public:
    VeDirectBatteryRole(Nmea2000ServiceInterface& nmea,
                        SerialSensorInterface& serial);

    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    VeDirectBatteryConfig config;

   private:
    void sendBatteryMetric(const VeDirectFrame& frame);

    Nmea2000ServiceInterface& nmea_;
    SerialSensorInterface& serial_;
    VeDirectParser parser_;
};
