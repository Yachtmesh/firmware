#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>
#include <memory>
#include <string>

#include "AisEncoder.h"
#include "WifiGateway0183Role.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service
#include "test_wifi_gateway_role.h"        // For FakeWifiService, FakeTcpServer, makeFakeTcp

// --- Config parsing tests ---

void test_wifi_gateway_0183_config_from_json() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

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

void test_wifi_gateway_0183_config_default_port() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(10110, role.config.port);
}

void test_wifi_gateway_0183_config_empty_ssid_fails() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "";
    doc["password"] = "pass";

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

void test_wifi_gateway_0183_config_json_roundtrip() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "MyBoat";
    doc["password"] = "sailaway";
    doc["port"] = 5555;

    role.configureFromJson(doc);

    StaticJsonDocument<256> outDoc;
    role.getConfigJson(outDoc);

    TEST_ASSERT_EQUAL_STRING("WifiGateway0183", outDoc["type"]);
    TEST_ASSERT_EQUAL_STRING("MyBoat", outDoc["ssid"]);
    TEST_ASSERT_EQUAL_STRING("sailaway", outDoc["password"]);
    TEST_ASSERT_EQUAL_UINT16(5555, outDoc["port"]);
}

void test_wifi_gateway_0183_type() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    TEST_ASSERT_EQUAL_STRING("WifiGateway0183", role.type());
}

// --- Lifecycle tests ---

void test_wifi_gateway_0183_registers_listener_on_start() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    TEST_ASSERT_EQUAL(0, nmea.listeners_.size());
    role.start();
    TEST_ASSERT_EQUAL(1, nmea.listeners_.size());
}

void test_wifi_gateway_0183_unregisters_listener_on_stop() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    TEST_ASSERT_EQUAL(1, nmea.listeners_.size());
    role.stop();
    TEST_ASSERT_EQUAL(0, nmea.listeners_.size());
}

void test_wifi_gateway_0183_connects_wifi_on_start() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "BoatNet";
    doc["password"] = "sail123";
    role.configureFromJson(doc);

    role.start();
    TEST_ASSERT_TRUE(wifi.connectCalled);
    TEST_ASSERT_EQUAL_STRING("BoatNet", wifi.lastSsid);
}

void test_wifi_gateway_0183_starts_tcp_when_wifi_connected() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    doc["port"] = 9999;
    role.configureFromJson(doc);

    role.start();
    role.loop();

    TEST_ASSERT_TRUE(tcpPtr->started);
    TEST_ASSERT_EQUAL_UINT16(9999, tcpPtr->lastPort);
}

void test_wifi_gateway_0183_stops_tcp_on_wifi_disconnect() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();
    role.loop();
    TEST_ASSERT_TRUE(tcpPtr->started);

    wifi.connected = false;
    role.loop();
    TEST_ASSERT_TRUE(tcpPtr->stopped);
}

// --- AIS message forwarding ---

void test_wifi_gateway_0183_forwards_ais_as_aivdm() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();

    // Build a PGN 129039 payload
    AisClassBPosition pos = {
        .mmsi = 211234567,
        .latitude = 52.0,
        .longitude = 4.3,
        .sog = 5.0,
        .cog = 180.0,
        .heading = 175.0,
        .seconds = 30};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    role.onN2kMessage(129039, 4, 22, n2kBuf, PGN_AIS_CLASS_B_SIZE);

    TEST_ASSERT_EQUAL(1, tcpPtr->sentData.size());
    // Should be an !AIVDM sentence
    TEST_ASSERT_TRUE(tcpPtr->sentData[0].find("!AIVDM") == 0);
}

void test_wifi_gateway_0183_ignores_unsupported_pgn() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();

    // Send an unsupported PGN (e.g. FluidLevel)
    unsigned char data[8] = {0};
    role.onN2kMessage(127505, 3, 22, data, sizeof(data));

    TEST_ASSERT_EQUAL(0, tcpPtr->sentData.size());
}

// --- Status: reason and getStatusJson ---

void test_wifi_gateway_0183_stop_sets_reason() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);
    role.start();
    role.stop();

    TEST_ASSERT_FALSE(role.status().running);
    TEST_ASSERT_FALSE(role.status().reason.empty());
}

void test_wifi_gateway_0183_start_clears_reason() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);
    role.start();

    TEST_ASSERT_TRUE(role.status().reason.empty());
}

void test_wifi_gateway_0183_status_json_includes_ip() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);
    role.start();

    strncpy(wifi.ip, "10.1.1.1", sizeof(wifi.ip) - 1);

    StaticJsonDocument<128> statusDoc;
    role.getStatusJson(statusDoc);

    TEST_ASSERT_EQUAL_STRING("10.1.1.1", statusDoc["ip"] | "");
}

void test_wifi_gateway_0183_forwards_static_data() {
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    auto [tcp, tcpPtr] = makeFakeTcp();
    WifiGateway0183Role role(nmea, wifi, std::move(tcp));

    StaticJsonDocument<256> doc;
    doc["ssid"] = "TestNet";
    doc["password"] = "pass";
    role.configureFromJson(doc);

    role.start();

    AisClassBStaticA staticData = {.mmsi = 211234567, .name = "TESTBOAT"};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_STATIC_A_SIZE];
    buildPgn129809(staticData, n2kBuf, sizeof(n2kBuf));

    role.onN2kMessage(129809, 6, 22, n2kBuf, PGN_AIS_CLASS_B_STATIC_A_SIZE);

    TEST_ASSERT_EQUAL(1, tcpPtr->sentData.size());
    TEST_ASSERT_TRUE(tcpPtr->sentData[0].find("!AIVDM") == 0);
}
