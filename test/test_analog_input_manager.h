#pragma once
#include <unity.h>

#include "AnalogInputManager.h"
#include "MockI2cBus.h"

void test_analog_input_manager_claim_returns_reader() {
    MockI2cBus bus;
    AnalogInputManager manager(bus);

    auto* reader = manager.claim(0x48, 2);

    TEST_ASSERT_NOT_NULL(reader);
}

void test_analog_input_manager_claim_same_channel_twice_fails() {
    MockI2cBus bus;
    AnalogInputManager manager(bus);

    manager.claim(0x48, 2);
    auto* second = manager.claim(0x48, 2);

    TEST_ASSERT_NULL(second);
}

void test_analog_input_manager_claim_different_channels_same_address_succeeds() {
    MockI2cBus bus;
    AnalogInputManager manager(bus);

    auto* ch2 = manager.claim(0x48, 2);
    auto* ch3 = manager.claim(0x48, 3);

    TEST_ASSERT_NOT_NULL(ch2);
    TEST_ASSERT_NOT_NULL(ch3);
    TEST_ASSERT_NOT_EQUAL(ch2, ch3);
}

void test_analog_input_manager_release_allows_reclaim() {
    MockI2cBus bus;
    AnalogInputManager manager(bus);

    manager.claim(0x48, 2);
    manager.release(0x48, 2);
    auto* reader = manager.claim(0x48, 2);

    TEST_ASSERT_NOT_NULL(reader);
}

void test_analog_input_manager_release_other_channel_does_not_free_target() {
    MockI2cBus bus;
    AnalogInputManager manager(bus);

    manager.claim(0x48, 2);
    manager.release(0x48, 3);  // Different channel, never claimed
    auto* second = manager.claim(0x48, 2);

    TEST_ASSERT_NULL(second);
}
