#include "FluidLevelSensorRole.h"

#ifdef ESP32
#include <esp_log.h>
static const char* TAG = "FluidLevel";
#else
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#endif

#include "NMEA2000Service.h"

static bool isValidI2cAddress(uint8_t addr) {
    return addr == 0x40 || addr == 0x41 || addr == 0x44 || addr == 0x45;
}

FluidLevelCalculator::FluidLevelCalculator(float minCurrent, float maxCurrent)
    : min_(minCurrent), max_(maxCurrent) {}

float FluidLevelCalculator::toPercent(float current) const {
    if (current <= min_) return 0.0f;
    if (current >= max_) return 100.0f;
    return (current - min_) / (max_ - min_) * 100.0f;
}

FluidLevelSensorRole::FluidLevelSensorRole(CurrentSensorManagerInterface& manager,
                                           Nmea2000ServiceInterface& nmea)
    : manager_(manager), nmea_(nmea) {}

const char* FluidLevelSensorRole::type() { return "FluidLevel"; }

void FluidLevelSensorRole::configure(const RoleConfig& cfg) {
    config = static_cast<const FluidLevelConfig&>(cfg);

    delete calculator_;
    calculator_ = new FluidLevelCalculator(config.minCurrent, config.maxCurrent);
}

bool FluidLevelSensorRole::validate() {
    if (config.minCurrent >= config.maxCurrent) return false;
    if (!isValidI2cAddress(config.i2cAddress)) return false;
    return true;
}

void FluidLevelSensorRole::start() {
    if (!validate()) return;

    sensor_ = manager_.claim(config.i2cAddress, config.shuntOhms);
    if (!sensor_) {
        ESP_LOGE(TAG, "Address 0x%02X already in use", config.i2cAddress);
        return;
    }
    status_.running = true;
}

void FluidLevelSensorRole::stop() {
    if (sensor_) {
        manager_.release(config.i2cAddress);
        sensor_ = nullptr;
    }
    status_.running = false;
}

void FluidLevelSensorRole::loop() {
    if (!status_.running || !sensor_ || !calculator_) return;

    auto r = sensor_->read();
    if (!r.valid) return;

    lastLevel = calculator_->toPercent(r.current);

    Metric metric{MetricType::FluidLevel, lastLevel};
    metric.context.fluidLevel.inst = config.inst;
    metric.context.fluidLevel.fluidType = config.fluidType;
    metric.context.fluidLevel.capacity = config.capacity;

    nmea_.sendMetric(metric);
}

void FluidLevelSensorRole::getConfigJson(JsonDocument& doc) { config.toJson(doc); }

bool FluidLevelSensorRole::configureFromJson(const JsonDocument& doc) {
    float minC = doc["minCurrent"] | 0.0f;
    float maxC = doc["maxCurrent"] | 0.0f;
    unsigned char inst = doc["inst"] | 0;
    const char* ftStr = doc["fluidType"] | "Unavailable";
    uint16_t cap = doc["capacity"] | 0;
    uint8_t addr = doc["i2cAddress"] | (uint8_t)0x40;
    float shunt = doc["shuntOhms"] | 0.1f;

    FluidLevelConfig newConfig(FluidTypeFromString(ftStr), inst, cap, minC, maxC, addr, shunt);
    configure(newConfig);
    return validate();
}

void FluidLevelConfig::toJson(JsonDocument& doc) const {
    doc["type"] = "FluidLevel";
    doc["fluidType"] = FluidTypeToString(fluidType);
    doc["inst"] = inst;
    doc["capacity"] = capacity;
    doc["minCurrent"] = minCurrent;
    doc["maxCurrent"] = maxCurrent;
    doc["i2cAddress"] = i2cAddress;
    doc["shuntOhms"] = shuntOhms;
}
