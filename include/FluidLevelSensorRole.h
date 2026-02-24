#pragma once
#include "CurrentSensorManager.h"
#include "CurrentSensorService.h"
#include "NMEA2000Service.h"
#include "Role.h"

class FluidLevelCalculator {
   public:
    FluidLevelCalculator(float minCurrent, float maxCurrent);
    float toPercent(float current) const;

   private:
    float min_, max_;
};

struct FluidLevelConfig : public RoleConfig {
    float minCurrent = 0.0f;
    float maxCurrent = 0.0f;
    unsigned char inst = 0;
    FluidType fluidType = FluidType::Unavailable;
    uint16_t capacity = 0;
    uint8_t i2cAddress = 0x40;
    float shuntOhms = 0.1f;

    FluidLevelConfig() = default;
    FluidLevelConfig(FluidType ft, unsigned char i, uint16_t cap, float minC, float maxC,
                     uint8_t addr = 0x40, float shunt = 0.1f)
        : minCurrent(minC),
          maxCurrent(maxC),
          inst(i),
          fluidType(ft),
          capacity(cap),
          i2cAddress(addr),
          shuntOhms(shunt) {}

    void toJson(JsonDocument& doc) const;
};

class FluidLevelSensorRole : public Role {
   public:
    FluidLevelSensorRole(CurrentSensorManagerInterface& manager, Nmea2000ServiceInterface& nmea);

    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    FluidLevelConfig config;

   private:
    CurrentSensorManagerInterface& manager_;
    Nmea2000ServiceInterface& nmea_;
    CurrentSensorInterface* sensor_ = nullptr;
    FluidLevelCalculator* calculator_ = nullptr;
    float lastLevel = 0.0f;
};
