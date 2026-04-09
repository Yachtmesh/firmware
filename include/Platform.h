#pragma once

#include <cstdint>
#include <string>

// Abstract interface for platform-specific operations
// Allows testing without hardware dependencies
class PlatformInterface {
   public:
    // Get Bluetooth MAC address (6 bytes)
    virtual void getMacAddress(uint8_t* mac) = 0;

    // Device ID persistence
    virtual std::string loadDeviceId() = 0;
    virtual void saveDeviceId(const std::string& id) = 0;

    // System information
    virtual float getCpuTemperature() = 0;
    virtual uint32_t getMillis() = 0;
    virtual uint32_t getFreeHeap() const = 0;
    virtual uint32_t getMinFreeHeap() const = 0;
    virtual uint8_t getCpuLoad() = 0;

    virtual ~PlatformInterface() = default;
};
