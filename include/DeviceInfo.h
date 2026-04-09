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

    // Build device info as a JSON string:
    // { "id": "HJ1DS2", "mac": "aa:bb:cc:dd:ee:ff", "nmea": 22,
    //   "fw": "0.1.0", "displayName": "Sensor Engine Room" }
    // displayName is passed in by the caller (owned by BluetoothService).
    std::string buildDeviceInfoJson(const std::string& displayName);

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

    // Data structure size
    static constexpr size_t STATUS_SIZE = 9;

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
