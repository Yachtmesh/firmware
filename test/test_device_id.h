#pragma once

#include <unity.h>

#include "DeviceId.h"

// Tests that deviceIdFromMac produces a 6-character alphanumeric ID
void test_device_id_from_mac_format() {
    uint8_t mac[] = {0xAA, 0xBB, 0x12, 0x34, 0x56, 0x78};

    std::string id = deviceIdFromMac(mac);

    TEST_ASSERT_EQUAL(6, id.length());
    for (char c : id) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        TEST_ASSERT_TRUE_MESSAGE(valid, "ID contains invalid character");
    }
}

// Tests that the same MAC always produces the same ID (deterministic)
void test_device_id_from_mac_deterministic() {
    uint8_t mac[] = {0x00, 0x11, 0xAA, 0xBB, 0xCC, 0xDD};

    std::string id1 = deviceIdFromMac(mac);
    std::string id2 = deviceIdFromMac(mac);

    TEST_ASSERT_EQUAL_STRING(id1.c_str(), id2.c_str());
}

// Tests that different MACs produce different IDs
void test_device_id_from_mac_differs_for_different_mac() {
    uint8_t macA[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t macB[] = {0x00, 0x11, 0xAA, 0xBB, 0xCC, 0xDD};

    std::string idA = deviceIdFromMac(macA);
    std::string idB = deviceIdFromMac(macB);

    TEST_ASSERT_TRUE(idA != idB);
}
