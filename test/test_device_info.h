#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include "DeviceInfo.h"
#include "MockPlatform.h"
#include "NMEA2000Service.h"

class MockNmea2000Service : public Nmea2000ServiceInterface {
   public:
    uint8_t address = 22;
    void sendMetric(const Metric&) override {}
    void start() override {}
    uint8_t getAddress() const override { return address; }
};

// Tests that DeviceInfo generates ID from MAC when no stored ID
void test_device_info_generates_id_from_mac() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    // Set a specific MAC address
    uint8_t mac[] = {0xAA, 0xBB, 0x12, 0x34, 0x56, 0x78};
    platform.setMacAddress(mac);

    DeviceInfo info(platform, nmea);

    std::string id = info.getDeviceId();
    TEST_ASSERT_EQUAL(6, id.length());
    TEST_ASSERT_TRUE(platform.wasSaveDeviceIdCalled());
}

// Tests that DeviceInfo ID is 6 characters alphanumeric
void test_device_info_id_format() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    DeviceInfo info(platform, nmea);

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
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("ABC123");

    DeviceInfo info(platform, nmea);

    TEST_ASSERT_EQUAL_STRING("ABC123", info.getDeviceId().c_str());
    TEST_ASSERT_FALSE(platform.wasSaveDeviceIdCalled());
}

// Tests that DeviceInfo regenerates invalid stored ID (wrong length)
void test_device_info_regenerates_invalid_id() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("TOO_LONG_ID");

    DeviceInfo info(platform, nmea);

    std::string id = info.getDeviceId();
    TEST_ASSERT_EQUAL(6, id.length());
    TEST_ASSERT_TRUE(platform.wasSaveDeviceIdCalled());
}

// Tests that buildDeviceInfoJson produces correct JSON fields
void test_device_info_build_device_info_json_format() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");
    uint8_t mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    platform.setMacAddress(mac);

    DeviceInfo info(platform, nmea);

    std::string json = info.buildDeviceInfoJson("My Sensor");

    StaticJsonDocument<256> doc;
    TEST_ASSERT_EQUAL(DeserializationError::Ok,
                      deserializeJson(doc, json));

    TEST_ASSERT_EQUAL_STRING("DEVICE", doc["id"] | "");
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", doc["mac"] | "");
    TEST_ASSERT_EQUAL_INT(22, doc["nmea"] | -1);
    TEST_ASSERT_EQUAL_STRING("0.1.0", doc["fw"] | "");
    TEST_ASSERT_EQUAL_STRING("My Sensor", doc["displayName"] | "");
}

// Tests that buildDeviceInfoJson includes empty displayName when not set
void test_device_info_build_device_info_json_empty_display_name() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");

    DeviceInfo info(platform, nmea);

    std::string json = info.buildDeviceInfoJson("");

    StaticJsonDocument<256> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_STRING("", doc["displayName"] | "unset");
}

// Tests that buildDeviceStatus produces correct 18-byte format
void test_device_info_build_device_status_format() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");
    platform.setTemperature(45.5f);
    platform.setMillis(0);
    platform.setFreeHeap(180000);
    platform.setMinFreeHeap(120000);
    platform.setCpuLoad(25);

    DeviceInfo info(platform, nmea);
    info.start();

    // Advance time by 5 seconds
    platform.setMillis(5000);

    uint8_t buffer[DeviceInfo::STATUS_SIZE];
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

    // Free heap (bytes 9-12, uint32)
    uint32_t freeHeap;
    memcpy(&freeHeap, buffer + 9, sizeof(uint32_t));
    TEST_ASSERT_EQUAL_UINT32(180000, freeHeap);

    // Min free heap (bytes 13-16, uint32)
    uint32_t minFreeHeap;
    memcpy(&minFreeHeap, buffer + 13, sizeof(uint32_t));
    TEST_ASSERT_EQUAL_UINT32(120000, minFreeHeap);

    // CPU load (byte 17)
    TEST_ASSERT_EQUAL_UINT8(25, buffer[17]);
}

// Tests that status sequence increments on each build
void test_device_info_status_sequence_increments() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");

    DeviceInfo info(platform, nmea);

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
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");
    platform.setMillis(1000);  // Start at 1 second

    DeviceInfo info(platform, nmea);
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
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");
    platform.setTemperature(55.5f);

    DeviceInfo info(platform, nmea);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.5f, info.getCpuTemperature());
}

// Tests that same MAC produces same device ID (deterministic)
void test_device_info_id_deterministic() {
    MockPlatform platform1;
    MockPlatform platform2;
    MockNmea2000Service nmea;

    uint8_t mac[] = {0x00, 0x11, 0xAA, 0xBB, 0xCC, 0xDD};
    platform1.setMacAddress(mac);
    platform2.setMacAddress(mac);

    DeviceInfo info1(platform1, nmea);
    DeviceInfo info2(platform2, nmea);

    TEST_ASSERT_EQUAL_STRING(info1.getDeviceId().c_str(),
                             info2.getDeviceId().c_str());
}

// Tests that buildDeviceInfoJson uses NMEA address from injected service
void test_device_info_nmea_address_from_service() {
    MockPlatform platform;
    MockNmea2000Service nmea;
    platform.setStoredDeviceId("DEVICE");

    nmea.address = 42;
    DeviceInfo info(platform, nmea);

    std::string json = info.buildDeviceInfoJson("");
    StaticJsonDocument<256> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_INT(42, doc["nmea"] | -1);
}
