#pragma once
#include "AnalogInputManager.h"
#include "AnalogInputService.h"
#include "NMEA2000Service.h"
#include "Role.h"

class FluidLevelCalculator {
   public:
    FluidLevelCalculator(float minVoltage, float maxVoltage);
    float toPercent(float voltage) const;

   private:
    float min_, max_;
};

struct FluidLevelConfig : public RoleConfig {
    float minVoltage = 0.0f;
    float maxVoltage = 0.0f;
    unsigned char inst = 0;
    FluidType fluidType = FluidType::Unavailable;
    uint16_t capacity = 0;
    uint8_t i2cAddress = 0x48;
    uint8_t channel = 0;

    FluidLevelConfig() = default;
    FluidLevelConfig(FluidType ft, unsigned char i, uint16_t cap, float minV, float maxV,
                     uint8_t addr = 0x48, uint8_t ch = 0)
        : minVoltage(minV),
          maxVoltage(maxV),
          inst(i),
          fluidType(ft),
          capacity(cap),
          i2cAddress(addr),
          channel(ch) {}

    void toJson(JsonDocument& doc) const;
};

class FluidLevelSensorRole : public Role {
   public:
    FluidLevelSensorRole(AnalogInputManagerInterface& manager, Nmea2000ServiceInterface& nmea);

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
    AnalogInputManagerInterface& manager_;
    Nmea2000ServiceInterface& nmea_;
    AnalogInputInterface* sensor_ = nullptr;
    FluidLevelCalculator* calculator_ = nullptr;
    float lastLevel = 0.0f;
};
