#pragma once
#include <unity.h>

#include "CurrentSensorService.h"
#include "MockI2cBus.h"

// INA219 register addresses (mirrored here for test assertions)
static constexpr uint8_t INA219_REG_CONFIG = 0x00;
static constexpr uint8_t INA219_REG_BUS_VOLTAGE = 0x02;
static constexpr uint8_t INA219_REG_POWER = 0x03;
static constexpr uint8_t INA219_REG_CURRENT = 0x04;
static constexpr uint8_t INA219_REG_CALIBRATION = 0x05;

static constexpr uint8_t TEST_ADDR = 0x40;

void test_current_sensor_writes_calibration_register() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    sensor.read();  // Triggers calibration on first call

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == TEST_ADDR && w.reg == INA219_REG_CALIBRATION) {
            uint16_t cal = (uint16_t)((w.data[0] << 8) | w.data[1]);
            // Cal = trunc(0.04096 / (0.001 * 0.1)) = trunc(409.6) = 409
            TEST_ASSERT_EQUAL_UINT16(409, cal);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Calibration register was not written");
}

void test_current_sensor_writes_config_register() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    sensor.read();

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == TEST_ADDR && w.reg == INA219_REG_CONFIG) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Config register was not written");
}

void test_current_sensor_calibrates_only_once() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    sensor.read();
    sensor.read();

    int calWrites = 0;
    for (auto& w : bus.writes) {
        if (w.addr == TEST_ADDR && w.reg == INA219_REG_CALIBRATION) calWrites++;
    }
    TEST_ASSERT_EQUAL_INT(1, calWrites);
}

void test_current_sensor_reads_current() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    // currentRaw = 1000, current = 1000 * 0.001 = 1.0A
    bus.setRegister(TEST_ADDR, INA219_REG_CURRENT, 1000);
    auto r = sensor.read();

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.current);
}

void test_current_sensor_reads_bus_voltage() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    // Bus voltage: (raw >> 3) * 4mV. For 12V: 12000mV / 4mV = 3000, raw = 3000 << 3
    bus.setRegister(TEST_ADDR, INA219_REG_BUS_VOLTAGE, (uint16_t)(3000 << 3));
    auto r = sensor.read();

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, r.voltage);
}

void test_current_sensor_reads_power() {
    MockI2cBus bus;
    CurrentSensorService sensor(bus, TEST_ADDR, 0.1f);

    // power = powerRaw * 20 * 0.001. For 12W: 12 / (20 * 0.001) = 600
    bus.setRegister(TEST_ADDR, INA219_REG_POWER, 600);
    auto r = sensor.read();

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, r.power);
}

void test_current_sensor_calibration_scales_with_shunt() {
    MockI2cBus bus;
    // shuntOhms = 0.05 → Cal = trunc(0.04096 / (0.001 * 0.05)) = trunc(819.2) = 819
    CurrentSensorService sensor(bus, TEST_ADDR, 0.05f);
    sensor.read();

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == TEST_ADDR && w.reg == INA219_REG_CALIBRATION) {
            uint16_t cal = (uint16_t)((w.data[0] << 8) | w.data[1]);
            TEST_ASSERT_EQUAL_UINT16(819, cal);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}
