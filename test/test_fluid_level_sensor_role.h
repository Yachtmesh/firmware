#pragma once

#include <unity.h>
#include <ArduinoJson.h>

#include "FluidLevelSensorRole.h"
#include "NMEA2000Service.h"

class FakeAnalogInput : public AnalogInputInterface {
   public:
    float voltage = 0.0f;

    float readVoltage() override { return voltage; }
};

class FakeNmea2000Service : public Nmea2000ServiceInterface {
   public:
    bool sent = false;
    Metric lastMetric{MetricType::FluidLevel, 0.0f};
    float lastPercent = -1;

    void start() override {}

    void sendMetric(const Metric& metric) override {
        sent = true;
        lastMetric = metric;
    }
};

void test_fluid_level_sensor_role_basic_flow() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;

    FluidLevelSensorRole role(analog, nmea);

    // Identity
    TEST_ASSERT_EQUAL_STRING("FluidLevel", role.id());

    // Configure
    FluidLevelConfig cfg{FluidType::FuelGasoline, 14, 257, 1.0f, 5.0f};
    role.configure(cfg);

    // Validation should pass
    TEST_ASSERT_TRUE(role.validate());

    // Simulate sensor input
    analog.voltage = 3.0f;  // mid-scale

    // Run one cycle
    role.start();
    role.loop();

    // NMEA message should have been sent
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_STRING(MetricType::FluidLevel, nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, nmea.lastMetric.value);
    TEST_ASSERT_EQUAL(14, nmea.lastMetric.context.fluidLevel.inst);
    TEST_ASSERT_EQUAL_UINT16(257, nmea.lastMetric.context.fluidLevel.capacity);
    TEST_ASSERT_EQUAL(FluidType::FuelGasoline,
                      nmea.lastMetric.context.fluidLevel.fluidType);
}

void test_fluid_level_sensor_role_get_config_json() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(analog, nmea);

    FluidLevelConfig cfg{FluidType::Water, 2, 150, 0.5f, 4.5f};
    role.configure(cfg);

    StaticJsonDocument<256> doc;
    role.getConfigJson(doc);

    TEST_ASSERT_EQUAL_STRING("FluidLevel", doc["type"]);
    TEST_ASSERT_EQUAL_STRING("Water", doc["fluidType"]);
    TEST_ASSERT_EQUAL(2, doc["inst"]);
    TEST_ASSERT_EQUAL(150, doc["capacity"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, doc["minVoltage"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.5f, doc["maxVoltage"]);
}

void test_fluid_level_sensor_role_configure_from_json() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(analog, nmea);

    // Start with initial config
    FluidLevelConfig initial{FluidType::Water, 0, 100, 1.0f, 5.0f};
    role.configure(initial);

    // Update via JSON
    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Fuel";
    doc["inst"] = 3;
    doc["capacity"] = 200;
    doc["minVoltage"] = 0.2;
    doc["maxVoltage"] = 4.8;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_TRUE(result);

    // Verify config was updated
    TEST_ASSERT_EQUAL(FluidType::Fuel, role.config.fluidType);
    TEST_ASSERT_EQUAL(3, role.config.inst);
    TEST_ASSERT_EQUAL(200, role.config.capacity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, role.config.minVoltage);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.8f, role.config.maxVoltage);
}

void test_fluid_level_sensor_role_configure_from_json_invalid() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(analog, nmea);

    // Invalid config: minVoltage >= maxVoltage
    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;
    doc["capacity"] = 100;
    doc["minVoltage"] = 5.0;
    doc["maxVoltage"] = 1.0;

    bool result = role.configureFromJson(doc);
    TEST_ASSERT_FALSE(result);
}
