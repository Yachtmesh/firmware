#pragma once
#include <cstdint>

#include "I2cBusService.h"

// Reading from a current/voltage/power sensor (INA219 implementation)
struct CurrentSensorReading {
    float voltage;  // Bus voltage in Volts
    float current;  // Current in Amps
    float power;    // Power in Watts
    bool valid;
};

class CurrentSensorInterface {
   public:
    virtual CurrentSensorReading read() = 0;
    virtual ~CurrentSensorInterface() = default;
};

// CurrentSensorService implements the INA219 I2C current sensor.
// Only uses I2cBusInterface, so it is native-testable.
class CurrentSensorService : public CurrentSensorInterface {
   public:
    CurrentSensorService(I2cBusInterface& bus, uint8_t address, float shuntOhms);
    CurrentSensorReading read() override;

   private:
    I2cBusInterface& bus_;
    uint8_t address_;
    float shuntOhms_;
    bool calibrated_ = false;

    void calibrate();
    int16_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint16_t value);
};
