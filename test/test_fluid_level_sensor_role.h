#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <algorithm>
#include <vector>

#include "FluidLevelSensorRole.h"
#include "MockAnalogInputManager.h"
#include "NMEA2000Service.h"

class FakeNmea2000Service : public Nmea2000ServiceInterface {
   public:
    uint8_t address = 22;
    bool sent = false;
    Metric lastMetric{MetricType::FluidLevel, 0.0f};
    float lastPercent = -1;
    std::vector<N2kListenerInterface*> listeners_;
    struct SentMsg {
        uint32_t pgn;
        uint8_t priority;
        std::vector<uint8_t> data;
    };
    std::vector<SentMsg> msgsSent;

    void start() override {}

    void sendMetric(const Metric& metric) override {
        sent = true;
        lastMetric = metric;
        unsigned char dummy[] = {0x00};
        for (auto* listener : listeners_) {
            listener->onN2kMessage(0, 3, 22, dummy, sizeof(dummy));
        }
    }

    void sendMsg(uint32_t pgn, uint8_t priority, const unsigned char* data,
                 size_t len) override {
        msgsSent.push_back(
            {pgn, priority, std::vector<uint8_t>(data, data + len)});
        for (auto* listener : listeners_) {
            listener->onN2kMessage(pgn, priority, 22, data, len);
        }
    }

    void addListener(N2kListenerInterface* listener) override {
        listeners_.push_back(listener);
    }

    void removeListener(N2kListenerInterface* listener) override {
        listeners_.erase(
            std::remove(listeners_.begin(), listeners_.end(), listener),
            listeners_.end());
    }

    void loop() override {}

    uint8_t getAddress() const override { return address; }
};

void test_fluid_level_sensor_role_basic_flow() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;

    FluidLevelSensorRole role(manager, nmea);

    TEST_ASSERT_EQUAL_STRING("FluidLevel", role.type());

    TEST_ASSERT_EQUAL_STRING("unknown", role.id());
    role.setInstanceId("FluidLevel-abc");
    TEST_ASSERT_EQUAL_STRING("FluidLevel-abc", role.id());

    // Configure: minVoltage=1.0, maxVoltage=5.0 → 3.0V gives 50%
    FluidLevelConfig cfg{FluidType::FuelGasoline, 14, 257, 1.0f, 5.0f, 0x48, 2};
    role.configure(cfg);

    TEST_ASSERT_TRUE(role.validate());

    manager.sensor.reading.voltage = 3.0f;  // mid-scale

    role.start();
    role.loop();

    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_STRING(MetricType::FluidLevel, nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, nmea.lastMetric.value);
    TEST_ASSERT_EQUAL(14, nmea.lastMetric.context.fluidLevel.inst);
    TEST_ASSERT_EQUAL_UINT16(257, nmea.lastMetric.context.fluidLevel.capacity);
    TEST_ASSERT_EQUAL(FluidType::FuelGasoline,
                      nmea.lastMetric.context.fluidLevel.fluidType);
}

void test_fluid_level_sensor_role_get_config_json() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 2, 150, 0.5f, 2.0f, 0x49, 1};
    role.configure(cfg);

    StaticJsonDocument<256> doc;
    role.getConfigJson(doc);

    TEST_ASSERT_EQUAL_STRING("FluidLevel", doc["type"]);
    TEST_ASSERT_EQUAL_STRING("Water", doc["fluidType"]);
    TEST_ASSERT_EQUAL(2, doc["inst"]);
    TEST_ASSERT_EQUAL(150, doc["capacity"]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, doc["minVoltage"]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, doc["maxVoltage"]);
    TEST_ASSERT_EQUAL(0x49, doc["i2cAddress"]);
    TEST_ASSERT_EQUAL(1, doc["channel"]);
}

void test_fluid_level_sensor_role_configure_from_json() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Fuel";
    doc["inst"] = 3;
    doc["capacity"] = 200;
    doc["minVoltage"] = 0.5;
    doc["maxVoltage"] = 2.5;
    doc["i2cAddress"] = 0x48;
    doc["channel"] = 2;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(FluidType::Fuel, role.config.fluidType);
    TEST_ASSERT_EQUAL(3, role.config.inst);
    TEST_ASSERT_EQUAL(200, role.config.capacity);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, role.config.minVoltage);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, role.config.maxVoltage);
    TEST_ASSERT_EQUAL(0x48, role.config.i2cAddress);
    TEST_ASSERT_EQUAL(2, role.config.channel);
}

void test_fluid_level_sensor_role_configure_from_json_invalid() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;
    doc["capacity"] = 100;
    doc["minVoltage"] = 2.0;
    doc["maxVoltage"] = 0.5;
    doc["i2cAddress"] = 0x48;
    doc["channel"] = 0;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}

void test_fluid_level_sensor_role_start_claims_sensor() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 0, 100, 0.5f, 2.0f, 0x48, 2};
    role.configure(cfg);
    role.start();

    TEST_ASSERT_TRUE(manager.claimCalled);
    TEST_ASSERT_EQUAL(0x48, manager.lastClaimedAddress);
    TEST_ASSERT_EQUAL(2, manager.lastClaimedChannel);
    TEST_ASSERT_TRUE(role.status().running);
}

void test_fluid_level_sensor_role_stop_releases_sensor() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 0, 100, 0.5f, 2.0f, 0x48, 2};
    role.configure(cfg);
    role.start();
    role.stop();

    TEST_ASSERT_TRUE(manager.releaseCalled);
    TEST_ASSERT_FALSE(role.status().running);
    TEST_ASSERT_FALSE(role.status().reason.empty());
}

void test_fluid_level_sensor_role_start_clears_reason() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 0, 100, 0.5f, 2.0f, 0x48, 2};
    role.configure(cfg);
    role.start();

    TEST_ASSERT_TRUE(role.status().running);
    TEST_ASSERT_TRUE(role.status().reason.empty());
}

void test_fluid_level_sensor_role_address_conflict() {
    MockAnalogInputManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    manager.returnNull = true;  // Simulate channel already taken

    FluidLevelConfig cfg{FluidType::Water, 0, 100, 0.5f, 2.0f, 0x48, 2};
    role.configure(cfg);
    role.start();

    TEST_ASSERT_FALSE(role.status().running);
}

// FluidType serialization tests

void test_fluid_type_to_string_all_values() {
    TEST_ASSERT_EQUAL_STRING("Fuel", FluidTypeToString(FluidType::Fuel));
    TEST_ASSERT_EQUAL_STRING("Water", FluidTypeToString(FluidType::Water));
    TEST_ASSERT_EQUAL_STRING("GrayWater", FluidTypeToString(FluidType::GrayWater));
    TEST_ASSERT_EQUAL_STRING("LiveWell", FluidTypeToString(FluidType::LiveWell));
    TEST_ASSERT_EQUAL_STRING("Oil", FluidTypeToString(FluidType::Oil));
    TEST_ASSERT_EQUAL_STRING("BlackWater", FluidTypeToString(FluidType::BlackWater));
    TEST_ASSERT_EQUAL_STRING("FuelGasoline", FluidTypeToString(FluidType::FuelGasoline));
    TEST_ASSERT_EQUAL_STRING("Error", FluidTypeToString(FluidType::Error));
    TEST_ASSERT_EQUAL_STRING("Unavailable", FluidTypeToString(FluidType::Unavailable));
}

void test_fluid_type_from_string_all_values() {
    TEST_ASSERT_EQUAL(FluidType::Fuel, FluidTypeFromString("Fuel"));
    TEST_ASSERT_EQUAL(FluidType::Water, FluidTypeFromString("Water"));
    TEST_ASSERT_EQUAL(FluidType::GrayWater, FluidTypeFromString("GrayWater"));
    TEST_ASSERT_EQUAL(FluidType::LiveWell, FluidTypeFromString("LiveWell"));
    TEST_ASSERT_EQUAL(FluidType::Oil, FluidTypeFromString("Oil"));
    TEST_ASSERT_EQUAL(FluidType::BlackWater, FluidTypeFromString("BlackWater"));
    TEST_ASSERT_EQUAL(FluidType::FuelGasoline, FluidTypeFromString("FuelGasoline"));
    TEST_ASSERT_EQUAL(FluidType::Error, FluidTypeFromString("Error"));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("Unavailable"));
}

void test_fluid_type_from_string_unknown_returns_unavailable() {
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("Unknown"));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString(""));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("fuel"));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("WATER"));
}

void test_fluid_type_round_trip() {
    TEST_ASSERT_EQUAL(FluidType::Fuel,
                      FluidTypeFromString(FluidTypeToString(FluidType::Fuel)));
    TEST_ASSERT_EQUAL(FluidType::Water,
                      FluidTypeFromString(FluidTypeToString(FluidType::Water)));
    TEST_ASSERT_EQUAL(FluidType::GrayWater,
                      FluidTypeFromString(FluidTypeToString(FluidType::GrayWater)));
    TEST_ASSERT_EQUAL(FluidType::LiveWell,
                      FluidTypeFromString(FluidTypeToString(FluidType::LiveWell)));
    TEST_ASSERT_EQUAL(FluidType::Oil,
                      FluidTypeFromString(FluidTypeToString(FluidType::Oil)));
    TEST_ASSERT_EQUAL(FluidType::BlackWater,
                      FluidTypeFromString(FluidTypeToString(FluidType::BlackWater)));
    TEST_ASSERT_EQUAL(FluidType::FuelGasoline,
                      FluidTypeFromString(FluidTypeToString(FluidType::FuelGasoline)));
    TEST_ASSERT_EQUAL(FluidType::Error,
                      FluidTypeFromString(FluidTypeToString(FluidType::Error)));
    TEST_ASSERT_EQUAL(FluidType::Unavailable,
                      FluidTypeFromString(FluidTypeToString(FluidType::Unavailable)));
}
