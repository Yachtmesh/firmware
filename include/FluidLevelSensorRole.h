
#pragma once
#include <all.h>
#include <Role.h>
#include <NMEA2000Service.h>

struct FluidLevelConfig : public RoleConfig
{
    float minVoltage = 0.0f;
    float maxVoltage = 0.0f;

    FluidLevelConfig(float minV, float maxV)
        : minVoltage(minV), maxVoltage(maxV) {}
};

struct FluidLevelStatus : public RoleStatus
{
    float percent = 0.0f;
    bool running = false;
};

class FluidLevelSensorRole : public Role
{
public:
    FluidLevelSensorRole(AnalogInputInterface &analog,
                         Nmea2000ServiceInterface &nmea);

    // Role interface
    const char *id() override;
    void configure(const RoleConfig &cfg) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    RoleStatus status() override;

private:
    // Dependencies (injected)
    AnalogInputInterface &analog;
    Nmea2000ServiceInterface &nmea;

    // Domain logic
    FluidLevelCalculator *calculator = nullptr;

    // Configuration
    float minVoltage = 0.0f;
    float maxVoltage = 0.0f;

    // State
    bool configured = false;
    bool running = false;
    float lastPercent = 0.0f;
};
