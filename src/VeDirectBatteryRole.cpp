#include "VeDirectBatteryRole.h"

static const char* TAG = "VeDirectBattery";

#ifdef ESP32
#include <esp_log.h>
#else
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#endif

#include <cstring>

VeDirectBatteryRole::VeDirectBatteryRole(Nmea2000ServiceInterface& nmea,
                                         SerialSensorInterface& serial)
    : nmea_(nmea), serial_(serial) {}

const char* VeDirectBatteryRole::type() { return "VeDirectBattery"; }

void VeDirectBatteryRole::configure(const RoleConfig& cfg) {
    config = static_cast<const VeDirectBatteryConfig&>(cfg);
}

bool VeDirectBatteryRole::validate() { return true; }

void VeDirectBatteryRole::start() {
    serial_.begin(19200, config.rxPin, config.txPin);
    parser_.reset();
    status_.running = true;
    status_.reason = "";
    ESP_LOGI(TAG, "Started inst=%u rx=%d tx=%d", config.inst, config.rxPin, config.txPin);
}

void VeDirectBatteryRole::stop() {
    status_.running = false;
    status_.reason = "Stopped";
}

void VeDirectBatteryRole::loop() {
    if (!status_.running) return;

    SerialReading r;
    while ((r = serial_.readLine()).valid) {
        ESP_LOGD(TAG, "RX line: %s", r.line.c_str());
        const char* tab = strchr(r.line.c_str(), '\t');
        if (!tab) continue;

        size_t klen = static_cast<size_t>(tab - r.line.c_str());
        if (klen >= 32) continue;
        char key[32];
        memcpy(key, r.line.c_str(), klen);
        key[klen] = '\0';

        const char* val = tab + 1;

        // When the checksum byte is '\n', readLine() consumes it as a line
        // terminator leaving val empty. Restore it so the checksum is correct.
        char newlineBuf[2] = {'\n', '\0'};
        const char* feedVal = (strcmp(key, "Checksum") == 0 && *val == '\0')
                                  ? newlineBuf
                                  : val;

        if (parser_.feedLine(key, feedVal)) {
            const VeDirectFrame& frame = parser_.getFrame();
            if (frame.checksumOk && frame.hasData) {
                sendBatteryMetric(frame);
            } else if (!frame.checksumOk) {
                ESP_LOGW(TAG, "VE.Direct checksum failed, discarding frame");
            }
            // checksumOk && !hasData: history block (H1-H30) — silently skip
        }
    }
}

void VeDirectBatteryRole::sendBatteryMetric(const VeDirectFrame& frame) {
    Metric metric{MetricType::BatteryStatus, 0.0f};
    metric.context.battery.inst       = config.inst;
    metric.context.battery.voltage    = frame.voltage;
    metric.context.battery.current    = frame.current;
    metric.context.battery.soc        = frame.soc;
    metric.context.battery.ttg        = frame.ttg;
    metric.context.battery.consumedAh = frame.consumedAh;
    nmea_.sendMetric(metric);
}

bool VeDirectBatteryRole::configureFromJson(const JsonDocument& doc) {
    uint8_t inst = doc["inst"]  | (uint8_t)0;
    int     rx   = doc["rxPin"] | 16;
    int     tx   = doc["txPin"] | 17;

    VeDirectBatteryConfig newConfig(inst, rx, tx);
    configure(newConfig);
    return validate();
}

void VeDirectBatteryRole::getConfigJson(JsonDocument& doc) { config.toJson(doc); }

void VeDirectBatteryConfig::toJson(JsonDocument& doc) const {
    doc["type"]  = "VeDirectBattery";
    doc["inst"]  = inst;
    doc["rxPin"] = rxPin;
    doc["txPin"] = txPin;
}
