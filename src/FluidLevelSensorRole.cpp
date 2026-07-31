#include "FluidLevelSensorRole.h"

#ifdef ESP32
#include <esp_log.h>
static const char* TAG = "FluidLevel";
#else
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#endif

#include "NMEA2000Service.h"

static bool isValidI2cAddress(uint8_t addr) {
    return addr == 0x48 || addr == 0x49 || addr == 0x4A || addr == 0x4B;
}

static bool isValidChannel(uint8_t channel) { return channel <= 3; }

FluidLevelCalculator::FluidLevelCalculator(float minVoltage, float maxVoltage)
    : min_(minVoltage), max_(maxVoltage) {}

float FluidLevelCalculator::toPercent(float voltage) const {
    if (voltage <= min_) return 0.0f;
    if (voltage >= max_) return 100.0f;
    return (voltage - min_) / (max_ - min_) * 100.0f;
}

FluidLevelSensorRole::FluidLevelSensorRole(AnalogInputManagerInterface& manager,
                                           Nmea2000ServiceInterface& nmea)
    : manager_(manager), nmea_(nmea) {}

const char* FluidLevelSensorRole::type() { return "FluidLevel"; }

void FluidLevelSensorRole::configure(const RoleConfig& cfg) {
    config = static_cast<const FluidLevelConfig&>(cfg);

    delete calculator_;
    calculator_ =
        new FluidLevelCalculator(config.minVoltage, config.maxVoltage);
}

bool FluidLevelSensorRole::validate() {
    if (config.minVoltage >= config.maxVoltage) return false;
    if (!isValidI2cAddress(config.i2cAddress)) return false;
    if (!isValidChannel(config.channel)) return false;
    return true;
}

void FluidLevelSensorRole::start() {
    if (!validate()) return;

    sensor_ = manager_.claim(config.i2cAddress, config.channel);
    if (!sensor_) {
        ESP_LOGE(TAG, "Address 0x%02X channel %d already in use",
                 config.i2cAddress, config.channel);
        return;
    }
    status_.running = true;
    status_.reason = "";
}

void FluidLevelSensorRole::stop() {
    if (sensor_) {
        manager_.release(config.i2cAddress, config.channel);
        sensor_ = nullptr;
    }
    status_.running = false;
    status_.reason = "Sensor not running";
}

void FluidLevelSensorRole::loop() {
    if (!status_.running || !sensor_ || !calculator_) return;

    auto r = sensor_->read();
    if (!r.valid) return;

    lastLevel = calculator_->toPercent(r.voltage);
    ESP_LOGI(TAG, "inst=%u addr=0x%02X ch=%d voltage=%.3fV level=%.1f%%",
             config.inst, config.i2cAddress, config.channel, r.voltage,
             lastLevel);

    Metric metric{MetricType::FluidLevel, lastLevel};
    metric.context.fluidLevel.inst = config.inst;
    metric.context.fluidLevel.fluidType = config.fluidType;
    metric.context.fluidLevel.capacity = config.capacity;

    nmea_.sendMetric(metric);
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
    uint8_t addr = doc["i2cAddress"] | (uint8_t)0x48;
    uint8_t channel = doc["channel"] | (uint8_t)2;

    FluidLevelConfig newConfig(FluidTypeFromString(ftStr), inst, cap, minV,
                               maxV, addr, channel);
    configure(newConfig);
    return validate();
}

void FluidLevelConfig::toJson(JsonDocument& doc) const {
    doc["type"] = "FluidLevel";
    doc["fluidType"] = FluidTypeToString(fluidType);
    doc["inst"] = inst;
    doc["capacity"] = capacity;
    doc["minVoltage"] = minVoltage;
    doc["maxVoltage"] = maxVoltage;
    doc["i2cAddress"] = i2cAddress;
    doc["channel"] = channel;
}
