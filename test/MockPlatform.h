#pragma once

#include "Platform.h"
#include <cstring>
#include <string>

// Mock platform for testing without hardware dependencies
class MockPlatform : public PlatformInterface {
   public:
    MockPlatform() {
        // Default MAC: 00:11:22:33:44:55
        mac_[0] = 0x00;
        mac_[1] = 0x11;
        mac_[2] = 0x22;
        mac_[3] = 0x33;
        mac_[4] = 0x44;
        mac_[5] = 0x55;
    }

    void getMacAddress(uint8_t* mac) override {
        memcpy(mac, mac_, 6);
    }

    std::string loadDeviceId() override {
        return storedDeviceId_;
    }

    void saveDeviceId(const std::string& id) override {
        storedDeviceId_ = id;
        saveDeviceIdCalled_ = true;
    }

    float getCpuTemperature() override {
        return temperature_;
    }

    uint32_t getMillis() override {
        return millis_;
    }

    // Test helpers - setters
    void setMacAddress(const uint8_t* mac) {
        memcpy(mac_, mac, 6);
    }

    void setStoredDeviceId(const std::string& id) {
        storedDeviceId_ = id;
    }

    void setTemperature(float temp) {
        temperature_ = temp;
    }

    void setMillis(uint32_t ms) {
        millis_ = ms;
    }

    // Test helpers - verification
    bool wasSaveDeviceIdCalled() const {
        return saveDeviceIdCalled_;
    }

    void resetSaveDeviceIdCalled() {
        saveDeviceIdCalled_ = false;
    }

   private:
    uint8_t mac_[6];
    std::string storedDeviceId_;
    float temperature_ = 25.0f;
    uint32_t millis_ = 0;
    bool saveDeviceIdCalled_ = false;
};
