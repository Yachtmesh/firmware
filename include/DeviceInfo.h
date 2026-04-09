#pragma once

#include <cstdint>
#include <string>

#include "NMEA2000Service.h"
#include "Platform.h"

// DeviceInfo handles device identity and status serialization
// Extracted from BluetoothService for testability
class DeviceInfo {
   public:
    DeviceInfo(PlatformInterface& platform, Nmea2000ServiceInterface& nmea);

    // Get 6-character alphanumeric device ID
    std::string getDeviceId() const;

    // Build 20-byte device info binary format:
    // [0-5]   Device ID (6 bytes, ASCII)
    // [6-11]  MAC address (6 bytes)
    // [12]    NMEA address (1 byte)
    // [13-15] Firmware version (3 bytes: major, minor, patch)
    // [16-19] Reserved (4 bytes, zeroed)
    void buildDeviceInfo(uint8_t* buffer);

    // Build 18-byte device status binary format:
    // [0]     Sequence number (1 byte, auto-increments)
    // [1-4]   CPU temperature (4 bytes, float32 LE, °C)
    // [5-8]   Uptime in seconds (4 bytes, uint32 LE)
    // [9-12]  Free heap bytes (4 bytes, uint32 LE)
    // [13-16] Min free heap bytes ever (4 bytes, uint32 LE)
    // [17]    CPU load % (1 byte, 0-100, approximate)
    void buildDeviceStatus(uint8_t* buffer);

    // Get current CPU temperature
    float getCpuTemperature();

    // Get uptime in seconds since start() was called
    uint32_t getUptime() const;

    // Record start time (call when service starts)
    void start();

    // Data structure sizes
    static constexpr size_t DEVICE_INFO_SIZE = 20;
    static constexpr size_t STATUS_SIZE = 18;

    // Firmware version
    static constexpr uint8_t FW_VERSION_MAJOR = 0;
    static constexpr uint8_t FW_VERSION_MINOR = 1;
    static constexpr uint8_t FW_VERSION_PATCH = 0;

   private:
    PlatformInterface& platform_;
    Nmea2000ServiceInterface& nmea_;
    std::string deviceId_;
    uint8_t statusSeq_ = 0;
    uint32_t startTime_ = 0;

    void loadOrGenerateDeviceId();
    std::string generateDeviceId();
};
