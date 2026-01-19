#pragma once
#include <string>

class AnalogInputInterface
{
public:
    virtual float readVoltage() = 0;
    virtual ~AnalogInputInterface() = default;
};

class AnalogInputService : public AnalogInputInterface
{
public:
    float readVoltage() override;
};

class FluidLevelCalculator
{
public:
    FluidLevelCalculator(float minV, float maxV);

    float toPercent(float voltage) const;

private:
    float minV;
    float maxV;
};

enum class MetricType : uint8_t
{
    FluidLevel,
};

enum class FluidType : uint8_t
{
    Fuel,         // fluid type is fuel
    Water,        // fluid type is water
    GrayWater,    // fluid type is gray water
    LiveWell,     // fluid type is live well
    Oil,          // fluid type is oil
    BlackWater,   // fluid type is black water
    FuelGasoline, // fluid type is gasoline fuel
    Error,        // error occurred
    Unavailable,  // unavailable
};

struct FluidLevelContext
{
    unsigned char inst;  // maps to instance, e.g. tank number
    FluidType fluidType; // maps to NMEA2000::N2kFluidType
    int16_t capacity;    // tank capacity
};

struct MetricContext
{
    union
    {
        FluidLevelContext fluidLevel;
    };
};

struct Metric
{
    MetricType type;
    float value;
    uint8_t priority = 3;
    MetricContext context;

    Metric(MetricType t,
           float v,
           uint8_t inst = 0,
           uint8_t prio = 3)
        : type(t), value(v), priority(prio) {}
};