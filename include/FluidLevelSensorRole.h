
#pragma once
#include <ArduinoJson.h>
#include <NMEA2000Service.h>
#include <Role.h>
#include <all.h>

struct FluidLevelConfig : public RoleConfig {
    float minVoltage = 0.0f;
    float maxVoltage = 0.0f;
    unsigned char inst = 0;  // instance, indicating fuel tank, bilge, etc.
    FluidType fluidType = FluidType::Unavailable;
    uint16_t capacity = 0;

    FluidLevelConfig() = default;
    FluidLevelConfig(FluidType ft, unsigned char i, uint16_t cap, float minV,
                     float maxV)
        : fluidType(ft),
          inst(i),
          capacity(cap),
          minVoltage(minV),
          maxVoltage(maxV) {}

    void toJson(JsonDocument& doc) const;
};

struct FluidLevelStatus : public RoleStatus {
    float percent = 0.0f;
};

class FluidLevelSensorRole : public Role {
   public:
    FluidLevelSensorRole(AnalogInputInterface& analog,
                         Nmea2000ServiceInterface& nmea);

    // Role interface
    const char* id() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    RoleStatus status() override;
    void getConfigJson(JsonDocument& doc) override;

    // Configuration (public for direct access)
    FluidLevelConfig config;

   private:
    // Dependencies (injected)
    AnalogInputInterface& analog;
    Nmea2000ServiceInterface& nmea;

    // Domain logic
    FluidLevelCalculator* calculator = nullptr;

    // State
    float lastLevel = 0.0f;  // Percentage
    bool running = false;
};
