#pragma once

#include <unity.h>

#include "DeviceInfo.h"
#include "MockPlatform.h"

// Tests that DeviceInfo generates ID from MAC when no stored ID
void test_device_info_generates_id_from_mac() {
    MockPlatform platform;
    // Set a specific MAC address
    uint8_t mac[] = {0xAA, 0xBB, 0x12, 0x34, 0x56, 0x78};
    platform.setMacAddress(mac);

    DeviceInfo info(platform);

    std::string id = info.getDeviceId();
    TEST_ASSERT_EQUAL(6, id.length());
    TEST_ASSERT_TRUE(platform.wasSaveDeviceIdCalled());
}

// Tests that DeviceInfo ID is 6 characters alphanumeric
void test_device_info_id_format() {
    MockPlatform platform;
    DeviceInfo info(platform);

    std::string id = info.getDeviceId();
    TEST_ASSERT_EQUAL(6, id.length());

    // All characters should be A-Z or 0-9
    for (char c : id) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        TEST_ASSERT_TRUE_MESSAGE(valid, "ID contains invalid character");
    }
}

// Tests that DeviceInfo loads existing ID from storage
void test_device_info_loads_stored_id() {
    MockPlatform platform;
    platform.setStoredDeviceId("ABC123");

    DeviceInfo info(platform);

    TEST_ASSERT_EQUAL_STRING("ABC123", info.getDeviceId().c_str());
    TEST_ASSERT_FALSE(platform.wasSaveDeviceIdCalled());
}

// Tests that DeviceInfo regenerates invalid stored ID (wrong length)
void test_device_info_regenerates_invalid_id() {
    MockPlatform platform;
    platform.setStoredDeviceId("TOO_LONG_ID");

    DeviceInfo info(platform);

    std::string id = info.getDeviceId();
    TEST_ASSERT_EQUAL(6, id.length());
    TEST_ASSERT_TRUE(platform.wasSaveDeviceIdCalled());
}

// Tests that buildDeviceInfo produces correct 20-byte format
void test_device_info_build_device_info_format() {
    MockPlatform platform;
    platform.setStoredDeviceId("DEVICE");
    uint8_t mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    platform.setMacAddress(mac);

    DeviceInfo info(platform);

    uint8_t buffer[20];
    info.buildDeviceInfo(buffer);

    // Device ID (bytes 0-5)
    TEST_ASSERT_EQUAL_UINT8('D', buffer[0]);
    TEST_ASSERT_EQUAL_UINT8('E', buffer[1]);
    TEST_ASSERT_EQUAL_UINT8('V', buffer[2]);
    TEST_ASSERT_EQUAL_UINT8('I', buffer[3]);
    TEST_ASSERT_EQUAL_UINT8('C', buffer[4]);
    TEST_ASSERT_EQUAL_UINT8('E', buffer[5]);

    // MAC address (bytes 6-11)
    TEST_ASSERT_EQUAL_UINT8(0x11, buffer[6]);
    TEST_ASSERT_EQUAL_UINT8(0x22, buffer[7]);
    TEST_ASSERT_EQUAL_UINT8(0x33, buffer[8]);
    TEST_ASSERT_EQUAL_UINT8(0x44, buffer[9]);
    TEST_ASSERT_EQUAL_UINT8(0x55, buffer[10]);
    TEST_ASSERT_EQUAL_UINT8(0x66, buffer[11]);

    // NMEA address (byte 12)
    TEST_ASSERT_EQUAL_UINT8(22, buffer[12]);

    // Firmware version (bytes 13-15)
    TEST_ASSERT_EQUAL_UINT8(0, buffer[13]);  // major
    TEST_ASSERT_EQUAL_UINT8(1, buffer[14]);  // minor
    TEST_ASSERT_EQUAL_UINT8(0, buffer[15]);  // patch

    // Reserved (bytes 16-19)
    TEST_ASSERT_EQUAL_UINT8(0, buffer[16]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[17]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[18]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[19]);
}

// Tests that buildDeviceStatus produces correct 9-byte format
void test_device_info_build_device_status_format() {
    MockPlatform platform;
    platform.setStoredDeviceId("DEVICE");
    platform.setTemperature(45.5f);
    platform.setMillis(0);

    DeviceInfo info(platform);
    info.start();

    // Advance time by 5 seconds
    platform.setMillis(5000);

    uint8_t buffer[9];
    info.buildDeviceStatus(buffer);

    // Sequence (byte 0) - starts at 0
    TEST_ASSERT_EQUAL_UINT8(0, buffer[0]);

    // Temperature (bytes 1-4, float32)
    float temp;
    memcpy(&temp, buffer + 1, sizeof(float));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.5f, temp);

    // Uptime (bytes 5-8, uint32) - 5 seconds
    uint32_t uptime;
    memcpy(&uptime, buffer + 5, sizeof(uint32_t));
    TEST_ASSERT_EQUAL_UINT32(5, uptime);
}

// Tests that status sequence increments on each build
void test_device_info_status_sequence_increments() {
    MockPlatform platform;
    platform.setStoredDeviceId("DEVICE");

    DeviceInfo info(platform);

    uint8_t buffer[9];

    info.buildDeviceStatus(buffer);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[0]);

    info.buildDeviceStatus(buffer);
    TEST_ASSERT_EQUAL_UINT8(1, buffer[0]);

    info.buildDeviceStatus(buffer);
    TEST_ASSERT_EQUAL_UINT8(2, buffer[0]);
}

// Tests that uptime calculation is correct
void test_device_info_uptime_calculation() {
    MockPlatform platform;
    platform.setStoredDeviceId("DEVICE");
    platform.setMillis(1000);  // Start at 1 second

    DeviceInfo info(platform);
    info.start();

    TEST_ASSERT_EQUAL_UINT32(0, info.getUptime());

    platform.setMillis(11000);  // 10 seconds later
    TEST_ASSERT_EQUAL_UINT32(10, info.getUptime());

    platform.setMillis(61000);  // 60 seconds total
    TEST_ASSERT_EQUAL_UINT32(60, info.getUptime());
}

// Tests that getCpuTemperature returns platform value
void test_device_info_cpu_temperature() {
    MockPlatform platform;
    platform.setStoredDeviceId("DEVICE");
    platform.setTemperature(55.5f);

    DeviceInfo info(platform);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.5f, info.getCpuTemperature());
}

// Tests that same MAC produces same device ID (deterministic)
void test_device_info_id_deterministic() {
    MockPlatform platform1;
    MockPlatform platform2;

    uint8_t mac[] = {0x00, 0x11, 0xAA, 0xBB, 0xCC, 0xDD};
    platform1.setMacAddress(mac);
    platform2.setMacAddress(mac);

    DeviceInfo info1(platform1);
    DeviceInfo info2(platform2);

    TEST_ASSERT_EQUAL_STRING(info1.getDeviceId().c_str(),
                             info2.getDeviceId().c_str());
}
