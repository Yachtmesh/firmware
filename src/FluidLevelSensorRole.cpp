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

    FluidLevelConfig newConfig(fluidTypeFromString(ftStr), inst, cap, minV,
                               maxV);

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

FluidType fluidTypeFromString(const char* str) {
    if (strcmp(str, "Fuel") == 0) return FluidType::Fuel;
    if (strcmp(str, "Water") == 0) return FluidType::Water;
    if (strcmp(str, "GrayWater") == 0) return FluidType::GrayWater;
    if (strcmp(str, "LiveWell") == 0) return FluidType::LiveWell;
    if (strcmp(str, "Oil") == 0) return FluidType::Oil;
    if (strcmp(str, "BlackWater") == 0) return FluidType::BlackWater;
    if (strcmp(str, "FuelGasoline") == 0) return FluidType::FuelGasoline;
    if (strcmp(str, "Error") == 0) return FluidType::Error;
    return FluidType::Unavailable;
}

const char* fluidTypeToString(FluidType ft) {
    switch (ft) {
        case FluidType::Fuel:
            return "Fuel";
        case FluidType::Water:
            return "Water";
        case FluidType::GrayWater:
            return "GrayWater";
        case FluidType::LiveWell:
            return "LiveWell";
        case FluidType::Oil:
            return "Oil";
        case FluidType::BlackWater:
            return "BlackWater";
        case FluidType::FuelGasoline:
            return "FuelGasoline";
        case FluidType::Error:
            return "Error";
        default:
            return "Unavailable";
    }
}