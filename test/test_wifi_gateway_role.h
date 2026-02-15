#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "ActisenseEncoder.h"
#include "FluidLevelSensorRole.h"
#include "WifiGatewayRole.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service, FakeAnalogInput

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

class FakeTcpServer : public TcpServerInterface {
   public:
    bool started = false;
    bool stopped = false;
    uint16_t lastPort = 0;
    std::vector<std::string> sentData;

    bool start(uint16_t port) override {
        started = true;
        lastPort = port;
        return true;
    }

    void stop() override {
        stopped = true;
        started = false;
    }

    void loop() override {}

    void sendToAll(const char* data, size_t len) override {
        sentData.emplace_back(data, len);
    }
};

// --- Config parsing tests ---

void test_wifi_gateway_config_from_json() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

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
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

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
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "";
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

void test_wifi_gateway_config_missing_ssid_fails() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

// --- Config JSON round-trip ---

void test_wifi_gateway_config_json_roundtrip() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

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
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    TEST_ASSERT_EQUAL_STRING("WifiGateway", role.type());
}

// --- Lifecycle tests ---

void test_wifi_gateway_registers_listener_on_start() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    TEST_ASSERT_EQUAL(0, nmea.listeners_.size());

    role.start();
    TEST_ASSERT_EQUAL(1, nmea.listeners_.size());

    N2kListenerInterface* expected = &role;
    TEST_ASSERT_EQUAL(expected, nmea.listeners_[0]);
}

void test_wifi_gateway_unregisters_listener_on_stop() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

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
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

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
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    role.stop();

    TEST_ASSERT_TRUE(wifi.disconnectCalled);
}

void test_wifi_gateway_starts_tcp_when_wifi_connected() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    doc["port"] = 9999;
    role.configureFromJson(doc);

    role.start();
    // WiFi is connected (FakeWifiService sets connected=true on connect)
    role.loop();

    TEST_ASSERT_TRUE(tcp.started);
    TEST_ASSERT_EQUAL_UINT16(9999, tcp.lastPort);
}

void test_wifi_gateway_stops_tcp_on_stop() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    role.loop();
    role.stop();

    TEST_ASSERT_TRUE(tcp.stopped);
}

void test_wifi_gateway_forwards_data_to_tcp() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();

    // Send raw N2K fields — role should Actisense-encode and forward
    unsigned char data[] = {0xAA, 0xBB, 0xCC};
    uint32_t pgn = 127505;
    uint8_t priority = 3;
    uint8_t source = 22;
    role.onN2kMessage(pgn, priority, source, data, sizeof(data));

    TEST_ASSERT_EQUAL(1, tcp.sentData.size());
    TEST_ASSERT_TRUE(tcp.sentData[0].size() > 0);

    // Verify it matches what encodeActisense produces
    unsigned char expected[512];
    size_t expectedLen = encodeActisense(pgn, priority, source, data,
                                         sizeof(data), expected, sizeof(expected));
    TEST_ASSERT_EQUAL(expectedLen, tcp.sentData[0].size());
    TEST_ASSERT_EQUAL_MEMORY(expected, tcp.sentData[0].data(), expectedLen);
}

void test_wifi_gateway_stops_tcp_on_wifi_disconnect() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    role.loop();
    TEST_ASSERT_TRUE(tcp.started);

    // Simulate WiFi dropping
    wifi.connected = false;
    role.loop();

    TEST_ASSERT_TRUE(tcp.stopped);
}

void test_wifi_gateway_restarts_tcp_on_wifi_reconnect() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    WifiGatewayRole role(nmea, wifi, tcp);

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    doc["port"] = 10110;
    role.configureFromJson(doc);

    role.start();
    role.loop();
    TEST_ASSERT_TRUE(tcp.started);

    // WiFi drops
    wifi.connected = false;
    role.loop();
    TEST_ASSERT_TRUE(tcp.stopped);

    // WiFi reconnects
    tcp.stopped = false;
    wifi.connected = true;
    role.loop();
    TEST_ASSERT_TRUE(tcp.started);
    TEST_ASSERT_EQUAL_UINT16(10110, tcp.lastPort);
}

// --- Integration: local echo from sensor to gateway ---

void test_wifi_gateway_receives_local_sensor_data() {
    // Shared NMEA service so sensor and gateway are co-located
    FakeNmea2000Service nmea;
    FakeAnalogInput analog;
    FakeWifiService wifi;
    FakeTcpServer tcp;

    FluidLevelSensorRole sensor(analog, nmea);
    WifiGatewayRole gateway(nmea, wifi, tcp);

    // Configure sensor
    FluidLevelConfig sensorCfg{FluidType::Fuel, 1, 200, 1.0f, 5.0f};
    sensor.configure(sensorCfg);
    sensor.start();

    // Configure and start gateway (registers as listener)
    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    gateway.configureFromJson(doc);
    gateway.start();

    // Simulate sensor reading
    analog.voltage = 3.0f;
    sensor.loop();

    // Gateway should have received data via local echo and forwarded to TCP
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL(1, tcp.sentData.size());
    TEST_ASSERT_TRUE(tcp.sentData[0].size() > 0);
}
