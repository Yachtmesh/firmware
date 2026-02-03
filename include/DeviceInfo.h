#pragma once

#include <cstdint>
#include <string>

#include "Platform.h"

// DeviceInfo handles device identity and status serialization
// Extracted from BluetoothService for testability
class DeviceInfo {
   public:
    explicit DeviceInfo(PlatformInterface& platform);

    // Get 6-character alphanumeric device ID
    std::string getDeviceId() const;

    // Build 20-byte device info binary format:
    // [0-5]   Device ID (6 bytes, ASCII)
    // [6-11]  MAC address (6 bytes)
    // [12]    NMEA address (1 byte)
    // [13-15] Firmware version (3 bytes: major, minor, patch)
    // [16-19] Reserved (4 bytes, zeroed)
    void buildDeviceInfo(uint8_t* buffer);

    // Build 9-byte device status binary format:
    // [0]     Sequence number (1 byte, auto-increments)
    // [1-4]   CPU temperature (4 bytes, float32)
    // [5-8]   Uptime in seconds (4 bytes, uint32)
    void buildDeviceStatus(uint8_t* buffer);

    // Get current CPU temperature
    float getCpuTemperature();

    // Get uptime in seconds since start() was called
    uint32_t getUptime() const;

    // Record start time (call when service starts)
    void start();

    // Data structure sizes
    static constexpr size_t DEVICE_INFO_SIZE = 20;
    static constexpr size_t STATUS_SIZE = 9;

    // Firmware version
    static constexpr uint8_t FW_VERSION_MAJOR = 0;
    static constexpr uint8_t FW_VERSION_MINOR = 1;
    static constexpr uint8_t FW_VERSION_PATCH = 0;

    // Default NMEA address (will be configurable later)
    static constexpr uint8_t DEFAULT_NMEA_ADDRESS = 22;

   private:
    PlatformInterface& platform_;
    std::string deviceId_;
    uint8_t statusSeq_ = 0;
    uint32_t startTime_ = 0;

    void loadOrGenerateDeviceId();
    std::string generateDeviceId();
};
