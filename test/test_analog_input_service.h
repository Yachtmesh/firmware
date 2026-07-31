#pragma once
#include <unity.h>

#include "AnalogInputService.h"
#include "MockI2cBus.h"

// ADS1115 register addresses (mirrored here for test assertions)
static constexpr uint8_t ADS1115_REG_CONVERSION = 0x00;
static constexpr uint8_t ADS1115_REG_CONFIG = 0x01;

static constexpr uint8_t ADS1115_TEST_ADDR = 0x48;

void test_ads1115_first_read_after_channel_select_is_invalid() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);

    auto r = chip.readChannel(2);

    TEST_ASSERT_FALSE(r.valid);
}

void test_ads1115_writes_config_register_on_channel_select() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);

    chip.readChannel(2);

    bool found = false;
    for (auto& w : bus.writes) {
        if (w.addr == ADS1115_TEST_ADDR && w.reg == ADS1115_REG_CONFIG) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Config register was not written");
}

void test_ads1115_config_register_selects_requested_channel() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);

    chip.readChannel(3);

    uint16_t config = 0;
    for (auto& w : bus.writes) {
        if (w.addr == ADS1115_TEST_ADDR && w.reg == ADS1115_REG_CONFIG) {
            config = (uint16_t)((w.data[0] << 8) | w.data[1]);
        }
    }
    // MUX field (bits 14:12) = 100 + channel = 0b111 for channel 3
    uint16_t mux = (config >> 12) & 0x07;
    TEST_ASSERT_EQUAL_UINT16(0x07, mux);
}

void test_ads1115_second_read_same_channel_returns_valid_reading() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);

    // 1.0V at ±4.096V PGA (125uV/LSB): raw = 1.0 / 0.000125 = 8000
    bus.setRegister(ADS1115_TEST_ADDR, ADS1115_REG_CONVERSION, 8000);

    chip.readChannel(2);              // First read: mux settling, invalid
    auto r = chip.readChannel(2);     // Second read: same channel, valid

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.voltage);
}

void test_ads1115_switching_channel_invalidates_next_read() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);

    bus.setRegister(ADS1115_TEST_ADDR, ADS1115_REG_CONVERSION, 8000);

    chip.readChannel(2);
    chip.readChannel(2);              // Settled on channel 2

    auto r = chip.readChannel(3);     // Switch to channel 3: settling again
    TEST_ASSERT_FALSE(r.valid);
}

void test_ads1115_channel_reader_delegates_to_chip() {
    MockI2cBus bus;
    Ads1115Chip chip(bus, ADS1115_TEST_ADDR);
    bus.setRegister(ADS1115_TEST_ADDR, ADS1115_REG_CONVERSION, 8000);

    Ads1115ChannelReader reader(chip, 1);
    reader.read();               // Settling
    auto r = reader.read();      // Valid

    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.voltage);
}
