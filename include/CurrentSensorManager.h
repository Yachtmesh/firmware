#pragma once
#include <cstdint>
#include <map>
#include <memory>

#include "CurrentSensorService.h"

// Abstract interface for current sensor lifecycle management.
// Roles depend on this so they remain testable with MockCurrentSensorManager.
class CurrentSensorManagerInterface {
   public:
    virtual CurrentSensorInterface* claim(uint8_t address, float shuntOhms) = 0;
    virtual void release(uint8_t address) = 0;
    virtual ~CurrentSensorManagerInterface() = default;
};

// Manages the lifecycle of CurrentSensorService instances, one per I2C address.
// Roles call claim() on start and release() on stop.
// claim() returns nullptr if the address is already in use by another role.
class CurrentSensorManager : public CurrentSensorManagerInterface {
   public:
    explicit CurrentSensorManager(I2cBusInterface& bus);

    CurrentSensorInterface* claim(uint8_t address, float shuntOhms) override;
    void release(uint8_t address) override;

   private:
    I2cBusInterface& bus_;
    std::map<uint8_t, std::unique_ptr<CurrentSensorInterface>> sensors_;
};
