#include "DeviceInfo.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

DeviceInfo::DeviceInfo(PlatformInterface& platform, Nmea2000ServiceInterface& nmea)
    : platform_(platform), nmea_(nmea) {
    loadOrGenerateDeviceId();
}

void DeviceInfo::loadOrGenerateDeviceId() {
    deviceId_ = platform_.loadDeviceId();

    if (deviceId_.empty() || deviceId_.length() != 6) {
        deviceId_ = generateDeviceId();
        platform_.saveDeviceId(deviceId_);
    }
}

std::string DeviceInfo::generateDeviceId() {
    uint8_t mac[6];
    platform_.getMacAddress(mac);

    // Use last 4 bytes (32 bits) - first bytes are manufacturer prefix
    uint32_t id = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    result.reserve(6);

    for (int i = 0; i < 6; i++) {
        result += charset[id % 36];
        id /= 36;
    }

    return result;
}

std::string DeviceInfo::getDeviceId() const {
    return deviceId_;
}

void DeviceInfo::start() {
    startTime_ = platform_.getMillis();
}

uint32_t DeviceInfo::getUptime() const {
    return (platform_.getMillis() - startTime_) / 1000;
}

float DeviceInfo::getCpuTemperature() {
    return platform_.getCpuTemperature();
}

std::string DeviceInfo::buildDeviceInfoJson(const std::string& displayName) {
    StaticJsonDocument<256> doc;

    doc["id"] = deviceId_;

    uint8_t mac[6];
    platform_.getMacAddress(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    doc["mac"] = macStr;

    doc["nmea"] = nmea_.getAddress();

    char fwStr[12];
    snprintf(fwStr, sizeof(fwStr), "%d.%d.%d",
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    doc["fw"] = fwStr;

    doc["displayName"] = displayName;

    std::string result;
    serializeJson(doc, result);
    return result;
}

void DeviceInfo::buildDeviceStatus(uint8_t* buffer) {
    memset(buffer, 0, STATUS_SIZE);

    // Sequence (1 byte)
    buffer[0] = statusSeq_++;

    // Temperature (4 bytes, float32)
    float temp = getCpuTemperature();
    memcpy(buffer + 1, &temp, sizeof(float));

    // Uptime (4 bytes, uint32)
    uint32_t uptime = getUptime();
    memcpy(buffer + 5, &uptime, sizeof(uint32_t));
}
