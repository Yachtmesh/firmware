#include "FluidLevelSensorRole.h"
#include "NMEA2000Service.h"
#include <stdexcept>

FluidLevelCalculator::FluidLevelCalculator(float minV, float maxV)
    : minV(minV), maxV(maxV)
{
}

float FluidLevelCalculator::toPercent(float v) const
{
    if (v <= minV)
        return 0.0f;
    if (v >= maxV)
        return 100.0f;
    return (v - minV) / (maxV - minV) * 100.0f;
}

FluidLevelSensorRole::FluidLevelSensorRole(AnalogInputInterface &analog,
                                           Nmea2000ServiceInterface &nmea)
    : analog(analog), nmea(nmea)
{
}

const char *FluidLevelSensorRole::id()
{
    return "FluidLevel";
}

void FluidLevelSensorRole::configure(const RoleConfig &cfg)
{
    const auto *c = static_cast<const FluidLevelConfig *>(&cfg); // pointer

    if (!c)
    {
        throw std::invalid_argument(
            "FluidLevelSensorRole received wrong config type");
    }

    minVoltage = c->minVoltage;
    maxVoltage = c->maxVoltage;
    inst = c->inst;
    fluidType = c->fluidType;
    capacity = c->capacity;

    delete calculator;
    calculator = new FluidLevelCalculator(minVoltage, maxVoltage);

    configured = true;
}

bool FluidLevelSensorRole::validate()
{
    if (!configured)
        return false;
    if (minVoltage >= maxVoltage)
        return false;

    return true;
}

void FluidLevelSensorRole::start()
{
    if (!validate())
        return;

    running = true;
}

void FluidLevelSensorRole::stop()
{
    running = false;
}

void FluidLevelSensorRole::loop()
{
    if (!running || !calculator)
        return;

    float voltage = analog.readVoltage();
    float percent = calculator->toPercent(voltage);

    lastLevel = percent;

    Metric metric{MetricType::FluidLevel, lastLevel};
    metric.context.fluidLevel.inst = inst;
    metric.context.fluidLevel.fluidType = fluidType;
    metric.context.fluidLevel.capacity = capacity;

    nmea.sendMetric(metric);
}

RoleStatus FluidLevelSensorRole::status()
{
    FluidLevelStatus s;

    s.percent = lastLevel;
    s.running = running;

    return s;
}