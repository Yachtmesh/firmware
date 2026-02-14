#include "DeviceInfo.h"

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

void DeviceInfo::buildDeviceInfo(uint8_t* buffer) {
    memset(buffer, 0, DEVICE_INFO_SIZE);

    // Device ID (6 bytes, ASCII)
    memcpy(buffer, deviceId_.c_str(), 6);

    // MAC address (6 bytes)
    platform_.getMacAddress(buffer + 6);

    // NMEA address (1 byte)
    buffer[12] = nmea_.getAddress();

    // Firmware version (3 bytes)
    buffer[13] = FW_VERSION_MAJOR;
    buffer[14] = FW_VERSION_MINOR;
    buffer[15] = FW_VERSION_PATCH;

    // Reserved (4 bytes) - already zeroed
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
