#pragma once

#include <unity.h>
#include "NMEA2000Service.h"
#include "FluidLevelSensorRole.h"

class FakeAnalogInput : public AnalogInputInterface
{
public:
    float voltage = 0.0f;

    float readVoltage() override
    {
        return voltage;
    }
};

class FakeNmea2000Service : public Nmea2000ServiceInterface
{
public:
    bool sent = false;
    Metric lastMetric{"", 0.0f};
    float lastPercent = -1;

    void loop() override {}
    void start(unsigned long serialBaud) override {}

    void sendFluidLevel(float percent) override
    {
        sent = true;
        lastPercent = percent;
    }

    void sendMetric(const Metric &metric) override
    {
        sent = true;
        lastMetric = metric;
    }
};

void test_fluid_level_sensor_role_basic_flow()
{
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;

    FluidLevelSensorRole role(analog, nmea);

    // Identity
    TEST_ASSERT_EQUAL_STRING("fluid_level", role.id());

    // Configure
    FluidLevelConfig cfg{1.0f, 5.0f};
    role.configure(cfg);

    // Validation should pass
    TEST_ASSERT_TRUE(role.validate());

    // Simulate sensor input
    analog.voltage = 3.0f; // mid-scale

    // Run one cycle
    role.start();
    role.loop();

    // NMEA message should have been sent
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_STRING("fluid_level", nmea.lastMetric.id.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, nmea.lastMetric.value);
}
