#include "WeatherStationRole.h"

WeatherStationRole::WeatherStationRole(EnvironmentalSensorInterface& sensor,
                                       Nmea2000ServiceInterface& nmea,
                                       PlatformInterface& platform)
    : sensor_(sensor), nmea_(nmea), platform_(platform) {}

const char* WeatherStationRole::type() { return "WeatherStation"; }

void WeatherStationRole::configure(const RoleConfig& cfg) {
    config = static_cast<const WeatherStationConfig&>(cfg);
}

bool WeatherStationRole::validate() {
    if (config.intervalMs < 100) return false;
    return true;
}

void WeatherStationRole::start() {
    if (!validate()) return;
    lastBroadcastMs_ = platform_.getMillis();
    status_.running = true;
    status_.reason = "";
}

void WeatherStationRole::stop() {
    status_.running = false;
    status_.reason = "Sensor not running";
}

void WeatherStationRole::loop() {
    if (!status_.running) return;

    uint32_t now = platform_.getMillis();
    if (now - lastBroadcastMs_ < config.intervalMs) return;
    lastBroadcastMs_ = now;

    auto reading = sensor_.read();
    if (!reading.valid) return;

    Metric metric{MetricType::Environmental, 0.0f};
    metric.context.environmental.inst        = config.inst;
    metric.context.environmental.temperature = reading.temperature;
    metric.context.environmental.humidity    = reading.humidity;
    metric.context.environmental.pressure    = reading.pressure;

    nmea_.sendMetric(metric);
}

void WeatherStationRole::getConfigJson(JsonDocument& doc) { config.toJson(doc); }

bool WeatherStationRole::configureFromJson(const JsonDocument& doc) {
    uint8_t  inst       = doc["inst"]       | (uint8_t)0;
    uint32_t intervalMs = doc["intervalMs"] | (uint32_t)2500;

    WeatherStationConfig newConfig(inst, intervalMs);
    configure(newConfig);
    return validate();
}

void WeatherStationConfig::toJson(JsonDocument& doc) const {
    doc["type"]       = "WeatherStation";
    doc["inst"]       = inst;
    doc["intervalMs"] = intervalMs;
}
