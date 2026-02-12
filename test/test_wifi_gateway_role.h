#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>

#include "WifiGatewayRole.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service

class FakeWifiService : public WifiServiceInterface {
   public:
    bool connectCalled = false;
    bool disconnectCalled = false;
    bool connected = false;
    char lastSsid[33] = {0};
    char lastPassword[65] = {0};

    bool connect(const char* ssid, const char* password) override {
        connectCalled = true;
        strncpy(lastSsid, ssid, sizeof(lastSsid) - 1);
        strncpy(lastPassword, password, sizeof(lastPassword) - 1);
        connected = true;
        return true;
    }

    void disconnect() override {
        disconnectCalled = true;
        connected = false;
    }

    bool isConnected() const override { return connected; }
};

// --- Config parsing tests ---

void test_wifi_gateway_config_from_json() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "BoatWifi";
    doc["password"] = "secret123";
    doc["port"] = 2222;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("BoatWifi", role.config.ssid);
    TEST_ASSERT_EQUAL_STRING("secret123", role.config.password);
    TEST_ASSERT_EQUAL_UINT16(2222, role.config.port);
}

void test_wifi_gateway_config_default_port() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(10110, role.config.port);
}

void test_wifi_gateway_config_empty_ssid_fails() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "";
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

void test_wifi_gateway_config_missing_ssid_fails() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

// --- Config JSON round-trip ---

void test_wifi_gateway_config_json_roundtrip() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "MyBoat";
    doc["password"] = "sailaway";
    doc["port"] = 5555;

    role.configureFromJson(doc);

    StaticJsonDocument<256> outDoc;
    role.getConfigJson(outDoc);

    TEST_ASSERT_EQUAL_STRING("WifiGateway", outDoc["type"]);
    TEST_ASSERT_EQUAL_STRING("MyBoat", outDoc["ssid"]);
    TEST_ASSERT_EQUAL_STRING("sailaway", outDoc["password"]);
    TEST_ASSERT_EQUAL_UINT16(5555, outDoc["port"]);
}

// --- Type identification ---

void test_wifi_gateway_type() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    TEST_ASSERT_EQUAL_STRING("WifiGateway", role.type());
}

// --- Seasmart encoding ---

void test_seasmart_encode_known_input() {
    unsigned char data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    char buffer[128];

    size_t len = encodeSeasmart(127505, 0x01, 8, data, 0x12345678, buffer,
                                sizeof(buffer));

    TEST_ASSERT_GREATER_THAN(0, len);
    // Verify format: $PCDIN,<PGN>,<timestamp>,<source>,<data>*<checksum>\r\n
    TEST_ASSERT_TRUE(strncmp(buffer, "$PCDIN,", 7) == 0);
    // Verify ends with checksum and \r\n
    TEST_ASSERT_TRUE(buffer[len - 1] == '\n');
    TEST_ASSERT_TRUE(buffer[len - 2] == '\r');
    // Verify '*' before checksum
    TEST_ASSERT_NOT_NULL(strchr(buffer, '*'));
}

void test_seasmart_encode_checksum() {
    unsigned char data[] = {0xAA, 0xBB};
    char buffer[128];

    size_t len =
        encodeSeasmart(130311, 0x02, 2, data, 0x00000001, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, len);

    // Parse out the checksum
    char* star = strchr(buffer, '*');
    TEST_ASSERT_NOT_NULL(star);
    unsigned int parsedChecksum;
    sscanf(star + 1, "%02X", &parsedChecksum);

    // Verify checksum by computing XOR of everything between '$' and '*'
    unsigned char computed = 0;
    for (const char* p = buffer + 1; p < star; p++) {
        computed ^= static_cast<unsigned char>(*p);
    }
    TEST_ASSERT_EQUAL_UINT8(computed, static_cast<unsigned char>(parsedChecksum));
}

void test_seasmart_encode_buffer_too_small() {
    unsigned char data[] = {0x01, 0x02, 0x03};
    char buffer[5];  // Way too small

    size_t len = encodeSeasmart(127505, 0x01, 3, data, 0, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, len);
}

// --- Listener registration ---

void test_wifi_gateway_registers_listener_on_start() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    TEST_ASSERT_EQUAL(0, nmea.listeners_.size());

    role.start();
    TEST_ASSERT_EQUAL(1, nmea.listeners_.size());

    // Verify the registered listener is the role (via N2kListenerInterface*)
    N2kListenerInterface* expected = &role;
    TEST_ASSERT_EQUAL(expected, nmea.listeners_[0]);
}

void test_wifi_gateway_unregisters_listener_on_stop() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    TEST_ASSERT_EQUAL(1, nmea.listeners_.size());

    role.stop();
    TEST_ASSERT_EQUAL(0, nmea.listeners_.size());
}

void test_wifi_gateway_connects_wifi_on_start() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "BoatNet";
    doc["password"] = "sail123";
    role.configureFromJson(doc);

    role.start();

    TEST_ASSERT_TRUE(wifi.connectCalled);
    TEST_ASSERT_EQUAL_STRING("BoatNet", wifi.lastSsid);
    TEST_ASSERT_EQUAL_STRING("sail123", wifi.lastPassword);
}

void test_wifi_gateway_disconnects_wifi_on_stop() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    WifiGatewayRole role(nmea, wifi);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    role.stop();

    TEST_ASSERT_TRUE(wifi.disconnectCalled);
}
