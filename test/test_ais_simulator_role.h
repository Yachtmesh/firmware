#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <string>

#include "AisSimulatorRole.h"
#include "MockPlatform.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service

// --- Type identification ---

void test_ais_simulator_type() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    TEST_ASSERT_EQUAL_STRING("AisSimulator", role.type());
}

// --- Config parsing ---

void test_ais_simulator_config_from_json() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 3000;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(3000, role.config.intervalMs);
}

void test_ais_simulator_config_defaults() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(5000, role.config.intervalMs);
}

void test_ais_simulator_config_json_roundtrip() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 2000;

    role.configureFromJson(doc);

    StaticJsonDocument<256> outDoc;
    role.getConfigJson(outDoc);

    TEST_ASSERT_EQUAL_STRING("AisSimulator", outDoc["type"]);
    TEST_ASSERT_EQUAL_UINT32(2000, outDoc["intervalMs"]);
}

// --- Lifecycle tests ---

void test_ais_simulator_start_sets_running() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    TEST_ASSERT_FALSE(role.status().running);
    role.start();
    TEST_ASSERT_TRUE(role.status().running);
}

void test_ais_simulator_stop_clears_running() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    role.start();
    role.stop();
    TEST_ASSERT_FALSE(role.status().running);
    TEST_ASSERT_FALSE(role.status().reason.empty());
}

void test_ais_simulator_start_clears_reason() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    role.start();
    TEST_ASSERT_TRUE(role.status().running);
    TEST_ASSERT_TRUE(role.status().reason.empty());
}

// --- Emission tests ---

void test_ais_simulator_sends_n2k_on_interval() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    platform.setMillis(0);
    role.start();
    role.loop();  // t=0, emits first boat

    // Each boat emit sends position + static data = 2 messages
    TEST_ASSERT_EQUAL(2, nmea.msgsSent.size());
    TEST_ASSERT_EQUAL_UINT32(PGN_AIS_CLASS_B_POSITION, nmea.msgsSent[0].pgn);
    TEST_ASSERT_EQUAL_UINT8(PGN_AIS_CLASS_B_PRIORITY, nmea.msgsSent[0].priority);
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_SIZE, nmea.msgsSent[0].data.size());
    TEST_ASSERT_EQUAL_UINT32(PGN_AIS_CLASS_B_STATIC_A, nmea.msgsSent[1].pgn);
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_STATIC_A_SIZE, nmea.msgsSent[1].data.size());
}

void test_ais_simulator_round_robin_boats() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    platform.setMillis(0);
    role.start();
    role.loop();  // emits boat 0 at t=0

    // Emit 4 more boats by advancing time
    for (int i = 1; i <= 4; i++) {
        platform.setMillis(i * 1000);  // 5000/5 = 1000ms per boat
        role.loop();
    }

    // 5 boats x 2 messages each = 10
    TEST_ASSERT_EQUAL(10, nmea.msgsSent.size());

    // Even indices = position, odd indices = static data
    for (int i = 0; i < 10; i += 2) {
        TEST_ASSERT_EQUAL_UINT32(PGN_AIS_CLASS_B_POSITION,
                                 nmea.msgsSent[i].pgn);
        TEST_ASSERT_EQUAL_UINT32(PGN_AIS_CLASS_B_STATIC_A,
                                 nmea.msgsSent[i + 1].pgn);
    }
}

void test_ais_simulator_no_send_before_interval() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    platform.setMillis(0);
    role.start();
    role.loop();  // emits at t=0

    size_t countAfterFirst = nmea.msgsSent.size();

    // Advance less than per-boat interval
    platform.setMillis(500);
    role.loop();

    TEST_ASSERT_EQUAL(countAfterFirst, nmea.msgsSent.size());
}

void test_ais_simulator_forwarded_to_listener() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    // Set up a listener (like WifiGateway would)
    struct TestListener : public N2kListenerInterface {
        struct Msg {
            uint32_t pgn;
            std::vector<uint8_t> data;
        };
        std::vector<Msg> received;
        void onN2kMessage(uint32_t pgn, uint8_t priority, uint8_t source,
                          const unsigned char* data, size_t len) override {
            received.push_back({pgn, std::vector<uint8_t>(data, data + len)});
        }
    };
    TestListener listener;
    nmea.addListener(&listener);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    platform.setMillis(0);
    role.start();
    role.loop();

    // Listener should have received both messages (position + static A)
    TEST_ASSERT_EQUAL(2, listener.received.size());
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_POSITION, listener.received[0].pgn);
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_SIZE, listener.received[0].data.size());
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_STATIC_A, listener.received[1].pgn);
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_STATIC_A_SIZE, listener.received[1].data.size());

    nmea.removeListener(&listener);
}

void test_ais_simulator_mmsi_in_payload() {
    FakeNmea2000Service nmea;
    MockPlatform platform;
    AisSimulatorRole role(nmea, platform);

    StaticJsonDocument<256> doc;
    doc["intervalMs"] = 5000;
    role.configureFromJson(doc);

    platform.setMillis(0);
    role.start();
    role.loop();

    // Extract MMSI from payload bytes 1-4 (little-endian)
    const auto& data = nmea.msgsSent[0].data;
    uint32_t mmsi = data[1] | (data[2] << 8) | (data[3] << 16) |
                    (data[4] << 24);
    // First boat MMSI is 244000001
    TEST_ASSERT_EQUAL_UINT32(244000001, mmsi);
}
