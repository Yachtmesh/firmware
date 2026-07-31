#include "AnalogInputService.h"

// ADS1115 register addresses
static constexpr uint8_t REG_CONVERSION = 0x00;
static constexpr uint8_t REG_CONFIG = 0x01;

// Config register field positions/values (ADS1115 datasheet §9.6.3)
static constexpr uint16_t OS_START_SINGLE = 0x8000;      // Bit 15: start a conversion
static constexpr uint16_t MUX_SINGLE_ENDED_BASE = 0x04;  // MUX = 100 + channel selects AINx vs GND
static constexpr int MUX_SHIFT = 12;
static constexpr uint16_t PGA_4_096V = 0x1;  // FS = ±4.096V
static constexpr int PGA_SHIFT = 9;
static constexpr uint16_t MODE_SINGLE_SHOT = 0x1 << 8;
static constexpr uint16_t DATA_RATE_128SPS = 0x4 << 5;
static constexpr uint16_t COMP_QUE_DISABLE = 0x3;

// PGA ±4.096V over 16-bit signed range: LSB = 4.096 / 32768
static constexpr float VOLTAGE_LSB = 0.000125f;

Ads1115Chip::Ads1115Chip(I2cBusInterface& bus, uint8_t address) : bus_(bus), address_(address) {}

void Ads1115Chip::writeRegister(uint8_t reg, uint16_t value) {
    uint8_t buf[2] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    bus_.writeBytes(address_, reg, buf, 2);
}

int16_t Ads1115Chip::readConversionRegister() {
    uint8_t buf[2] = {};
    bus_.readBytes(address_, REG_CONVERSION, buf, 2);
    return (int16_t)((buf[0] << 8) | buf[1]);
}

void Ads1115Chip::startConversion(uint8_t channel) {
    uint16_t config = OS_START_SINGLE |
                      ((MUX_SINGLE_ENDED_BASE | channel) << MUX_SHIFT) |
                      (PGA_4_096V << PGA_SHIFT) | MODE_SINGLE_SHOT |
                      DATA_RATE_128SPS | COMP_QUE_DISABLE;
    writeRegister(REG_CONFIG, config);
}

AnalogInputReading Ads1115Chip::readChannel(uint8_t channel) {
    if (!activeChannelSet_ || activeChannel_ != channel) {
        startConversion(channel);
        activeChannel_ = channel;
        activeChannelSet_ = true;
        return AnalogInputReading{0.0f, false};
    }

    startConversion(channel);
    int16_t raw = readConversionRegister();
    return AnalogInputReading{(float)raw * VOLTAGE_LSB, true};
}

Ads1115ChannelReader::Ads1115ChannelReader(Ads1115Chip& chip, uint8_t channel)
    : chip_(chip), channel_(channel) {}

AnalogInputReading Ads1115ChannelReader::read() { return chip_.readChannel(channel_); }
