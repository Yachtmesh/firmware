#include "FluidLevelSensorRole.h"

#include "NMEA2000Service.h"
#include "RoleConfigFactory.h"

FluidLevelCalculator::FluidLevelCalculator(float minV, float maxV)
    : minV(minV), maxV(maxV) {}

float FluidLevelCalculator::toPercent(float v) const {
    if (v <= minV) return 0.0f;
    if (v >= maxV) return 100.0f;
    return (v - minV) / (maxV - minV) * 100.0f;
}

FluidLevelSensorRole::FluidLevelSensorRole(AnalogInputInterface& analog,
                                           Nmea2000ServiceInterface& nmea)
    : analog(analog), nmea(nmea) {}

const char* FluidLevelSensorRole::id() { return "FluidLevel"; }

void FluidLevelSensorRole::configure(const RoleConfig& cfg) {
    config = static_cast<const FluidLevelConfig&>(cfg);

    delete calculator;
    calculator = new FluidLevelCalculator(config.minVoltage, config.maxVoltage);
}

bool FluidLevelSensorRole::validate() {
    if (config.minVoltage >= config.maxVoltage) return false;

    return true;
}

void FluidLevelSensorRole::start() {
    if (!validate()) return;

    running = true;
}

void FluidLevelSensorRole::stop() { running = false; }

void FluidLevelSensorRole::loop() {
    if (!running || !calculator) return;

    float voltage = analog.readVoltage();
    float percent = calculator->toPercent(voltage);

    lastLevel = percent;

    Metric metric{MetricType::FluidLevel, lastLevel};
    metric.context.fluidLevel.inst = config.inst;
    metric.context.fluidLevel.fluidType = config.fluidType;
    metric.context.fluidLevel.capacity = config.capacity;

    nmea.sendMetric(metric);
}

RoleStatus FluidLevelSensorRole::status() {
    FluidLevelStatus s;

    s.percent = lastLevel;
    s.running = running;

    return s;
}

void FluidLevelSensorRole::getConfigJson(JsonDocument& doc) {
    config.toJson(doc);
}

bool FluidLevelSensorRole::configureFromJson(const JsonDocument& doc) {
    float minV = doc["minVoltage"] | 0.0f;
    float maxV = doc["maxVoltage"] | 0.0f;
    unsigned char inst = doc["inst"] | 0;
    const char* ftStr = doc["fluidType"] | "Unavailable";
    uint16_t cap = doc["capacity"] | 0;

    FluidLevelConfig newConfig(
        fluidTypeFromString(ftStr), inst, cap, minV, maxV);

    configure(newConfig);
    return validate();
}

void FluidLevelConfig::toJson(JsonDocument& doc) const {
    doc["type"] = "FluidLevel";
    doc["fluidType"] = fluidTypeToString(fluidType);
    doc["inst"] = inst;
    doc["capacity"] = capacity;
    doc["minVoltage"] = minVoltage;
    doc["maxVoltage"] = maxVoltage;
}