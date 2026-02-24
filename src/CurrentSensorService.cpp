#include "CurrentSensorService.h"

#include <cmath>

// INA219 register addresses
static constexpr uint8_t REG_CONFIG = 0x00;
static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;
static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
static constexpr uint8_t REG_POWER = 0x03;
static constexpr uint8_t REG_CURRENT = 0x04;
static constexpr uint8_t REG_CALIBRATION = 0x05;

// 1mA per LSB for current register
static constexpr float CURRENT_LSB = 0.001f;

// Calibration factor from INA219 datasheet §8.5: Cal = 0.04096 / (currentLsb * shuntOhms)
static constexpr float INA219_CAL_FACTOR = 0.04096f;

// CONFIG register: 32V bus range | PGA÷8 shunt range | 12-bit ADC | continuous shunt+bus
static constexpr uint16_t INA219_CONFIG_DEFAULT = 0x399F;

// Bus voltage register: data in bits [15:3], LSB = 4 mV
static constexpr int BUS_VOLTAGE_DATA_SHIFT = 3;
static constexpr float BUS_VOLTAGE_LSB_V = 0.004f;  // 4 mV per count

// Power register LSB = 20 × currentLsb (INA219 datasheet §8.5)
static constexpr float POWER_LSB_MULTIPLIER = 20.0f;

CurrentSensorService::CurrentSensorService(I2cBusInterface& bus, uint8_t address,
                                           float shuntOhms)
    : bus_(bus), address_(address), shuntOhms_(shuntOhms) {}

void CurrentSensorService::calibrate() {
    uint16_t cal = (uint16_t)(INA219_CAL_FACTOR / (CURRENT_LSB * shuntOhms_));
    writeRegister(REG_CALIBRATION, cal);
    writeRegister(REG_CONFIG, INA219_CONFIG_DEFAULT);
    calibrated_ = true;
}

int16_t CurrentSensorService::readRegister(uint8_t reg) {
    uint8_t buf[2] = {};
    bus_.readBytes(address_, reg, buf, 2);
    return (int16_t)((buf[0] << 8) | buf[1]);
}

void CurrentSensorService::writeRegister(uint8_t reg, uint16_t value) {
    uint8_t buf[2] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    bus_.writeBytes(address_, reg, buf, 2);
}

CurrentSensorReading CurrentSensorService::read() {
    if (!calibrated_) {
        calibrate();
    }

    CurrentSensorReading r{};

    int16_t busRaw = readRegister(REG_BUS_VOLTAGE);
    r.voltage = (float)(busRaw >> BUS_VOLTAGE_DATA_SHIFT) * BUS_VOLTAGE_LSB_V;

    int16_t currentRaw = readRegister(REG_CURRENT);
    r.current = (float)currentRaw * CURRENT_LSB;

    int16_t powerRaw = readRegister(REG_POWER);
    r.power = (float)(uint16_t)powerRaw * POWER_LSB_MULTIPLIER * CURRENT_LSB;

    r.valid = true;
    return r;
}
