#pragma once

#include "Platform.h"

// ESP32-specific implementation of PlatformInterface
class Esp32Platform : public PlatformInterface {
   public:
    void getMacAddress(uint8_t* mac) override;
    std::string loadDeviceId() override;
    void saveDeviceId(const std::string& id) override;
    float getCpuTemperature() override;
    uint32_t getMillis() override;

   private:
    static constexpr const char* NVS_NAMESPACE = "yachtmesh";
    static constexpr const char* NVS_DEVICE_ID_KEY = "ble_device_id";
};
