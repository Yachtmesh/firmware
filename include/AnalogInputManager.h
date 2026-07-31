#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <utility>

#include "AnalogInputService.h"

// Abstract interface for analog input channel lifecycle management.
// Roles depend on this so they remain testable with MockAnalogInputManager.
class AnalogInputManagerInterface {
   public:
    virtual AnalogInputInterface* claim(uint8_t address, uint8_t channel) = 0;
    virtual void release(uint8_t address, uint8_t channel) = 0;
    virtual ~AnalogInputManagerInterface() = default;
};

// Manages the lifecycle of Ads1115Chip instances, one per I2C address, and
// hands out per-channel readers so multiple Roles can share one physical
// ADS1115 chip on different channels.
// Roles call claim() on start and release() on stop.
// claim() returns nullptr if the (address, channel) pair is already in use.
class AnalogInputManager : public AnalogInputManagerInterface {
   public:
    explicit AnalogInputManager(I2cBusInterface& bus);

    AnalogInputInterface* claim(uint8_t address, uint8_t channel) override;
    void release(uint8_t address, uint8_t channel) override;

   private:
    I2cBusInterface& bus_;
    std::map<uint8_t, std::unique_ptr<Ads1115Chip>> chips_;
    std::map<std::pair<uint8_t, uint8_t>, std::unique_ptr<Ads1115ChannelReader>> readers_;
};
