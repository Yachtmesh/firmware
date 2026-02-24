#pragma once
#include <unity.h>

#include "EnvironmentalSensorService.h"
#include "MockI2cBus.h"

static constexpr uint8_t BME_ADDR = 0x76;

// BME280 register addresses mirrored for assertions
static constexpr uint8_t BME_REG_CHIP_ID   = 0xD0;
static constexpr uint8_t BME_REG_RESET     = 0xE0;
static constexpr uint8_t BME_REG_CTRL_HUM  = 0xF2;
static constexpr uint8_t BME_REG_CTRL_MEAS = 0xF4;
static constexpr uint8_t BME_REG_DATA      = 0xF7;

static constexpr uint8_t BME_CHIP_ID_CORRECT = 0x60;
static constexpr uint8_t BME_RESET_CMD_VAL   = 0xB6;

// --- Helpers ---

static void bmeSetChipId(MockI2cBus& bus, uint8_t id = BME_CHIP_ID_CORRECT) {
    bus.setRegisterByte(BME_ADDR, BME_REG_CHIP_ID, id);
}

// Reference calibration: Bosch datasheet example values
//   dig_T1=27504, dig_T2=26435, dig_T3=-1000
// With adc_T=519888 → compensateTemp returns 2508 (25.08 °C)
static void bmeLoadReferenceCalibration(MockI2cBus& bus) {
    uint8_t cal_tp[24] = {
        0x70, 0x6B,              // dig_T1 = 27504 (little-endian)
        0x43, 0x67,              // dig_T2 = 26435
        0x18, 0xFC,              // dig_T3 = -1000 (0xFC18 two's complement)
        0, 0, 0, 0, 0, 0, 0, 0, // dig_P1–P4 = 0
        0, 0, 0, 0, 0, 0, 0, 0, // dig_P5–P8 = 0
        0, 0,                    // dig_P9 = 0
    };
    bus.setRegisterBytes(BME_ADDR, 0x88, cal_tp, 24);

    bus.setRegisterByte(BME_ADDR, 0xA1, 0);  // dig_H1 = 0
    uint8_t cal_h[7] = {0, 0, 0, 0, 0, 0, 0};
    bus.setRegisterBytes(BME_ADDR, 0xE1, cal_h, 7);
}

// ADC data for adc_T=519888 (≈25 °C with reference calibration)
// Data layout in REG_DATA: press[2:0], temp[2:0], hum[1:0]
// 519888 = 0x7EED0 → data[3]=0x7E, data[4]=0xED, data[5]=0x00
static void bmeLoadTemperatureData(MockI2cBus& bus) {
    uint8_t data[8] = {0, 0, 0, 0x7E, 0xED, 0x00, 0, 0};
    bus.setRegisterBytes(BME_ADDR, BME_REG_DATA, data, 8);
}

// --- Tests ---

void test_environmental_sensor_init_writes_reset() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    sensor.read();

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == BME_ADDR && w.reg == BME_REG_RESET) {
            TEST_ASSERT_EQUAL_UINT8(BME_RESET_CMD_VAL, w.data[0]);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "reset register not written");
}

void test_environmental_sensor_init_writes_ctrl_hum() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    sensor.read();

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == BME_ADDR && w.reg == BME_REG_CTRL_HUM) {
            // Humidity oversampling ×1: bits[2:0] = 001
            TEST_ASSERT_EQUAL_UINT8(0x01, w.data[0]);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "ctrl_hum not written");
}

void test_environmental_sensor_init_writes_ctrl_meas() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    sensor.read();

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == BME_ADDR && w.reg == BME_REG_CTRL_MEAS) {
            // Temp ×1 (bits[7:5]=001), pressure ×1 (bits[4:2]=001), normal mode (bits[1:0]=11)
            TEST_ASSERT_EQUAL_UINT8(0x27, w.data[0]);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "ctrl_meas not written");
}

void test_environmental_sensor_wrong_chip_id_returns_invalid() {
    MockI2cBus bus;
    bmeSetChipId(bus, 0x55);  // Wrong chip ID
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    auto r = sensor.read();

    TEST_ASSERT_FALSE(r.valid);
}

void test_environmental_sensor_init_only_once() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    sensor.read();
    sensor.read();

    int ctrlMeasWrites = 0;
    for (auto& w : bus.writes) {
        if (w.addr == BME_ADDR && w.reg == BME_REG_CTRL_MEAS) ctrlMeasWrites++;
    }
    TEST_ASSERT_EQUAL_INT(1, ctrlMeasWrites);
}

void test_environmental_sensor_reads_temperature() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    bmeLoadReferenceCalibration(bus);
    bmeLoadTemperatureData(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    auto r = sensor.read();

    // dig_T1=27504, dig_T2=26435, dig_T3=-1000, adc_T=519888 → 25.08 °C
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 25.08f, r.temperature);
}

void test_environmental_sensor_result_valid_after_init() {
    MockI2cBus bus;
    bmeSetChipId(bus);
    bmeLoadReferenceCalibration(bus);
    bmeLoadTemperatureData(bus);
    EnvironmentalSensorService sensor(bus, BME_ADDR);

    auto r = sensor.read();

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.humidity >= 0.0f);
    TEST_ASSERT_TRUE(r.pressure >= 0.0f);
}
