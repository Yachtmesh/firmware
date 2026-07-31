#pragma once
#include <cstdint>

#include "I2cBusService.h"

// Reading from a single-channel analog input (ADS1115 ADC implementation)
struct AnalogInputReading {
    float voltage;  // Voltage in Volts
    bool valid;
};

class AnalogInputInterface {
   public:
    virtual AnalogInputReading read() = 0;
    virtual ~AnalogInputInterface() = default;
};

// Ads1115Chip drives a single ADS1115 ADC over I2C. One chip exposes four
// single-ended channels (AN0-AN3); Ads1115ChannelReader binds one channel to
// the AnalogInputInterface contract that Roles depend on.
//
// Only uses I2cBusInterface, so it is native-testable.
class Ads1115Chip {
   public:
    Ads1115Chip(I2cBusInterface& bus, uint8_t address);

    // Reads the given channel (0-3). The ADS1115 multiplexes one ADC across
    // all four channels, so switching channels requires starting a new
    // conversion; the first read() after a channel switch returns
    // valid=false while that conversion settles.
    AnalogInputReading readChannel(uint8_t channel);

   private:
    I2cBusInterface& bus_;
    uint8_t address_;
    bool activeChannelSet_ = false;
    uint8_t activeChannel_ = 0;

    void startConversion(uint8_t channel);
    int16_t readConversionRegister();
    void writeRegister(uint8_t reg, uint16_t value);
};

class Ads1115ChannelReader : public AnalogInputInterface {
   public:
    Ads1115ChannelReader(Ads1115Chip& chip, uint8_t channel);
    AnalogInputReading read() override;

   private:
    Ads1115Chip& chip_;
    uint8_t channel_;
};
