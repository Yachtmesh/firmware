#pragma once
#include <ArduinoJson.h>
#include <unity.h>

#include "MockEnvironmentalSensorService.h"
#include "MockPlatform.h"
#include "WeatherStationRole.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service

// ─── Helpers ────────────────────────────────────────────────────────────────

static WeatherStationConfig makeWeatherConfig() {
    // Realistic non-identity values — see mutation-testing skill
    return WeatherStationConfig{/*inst=*/0, /*intervalMs=*/2500};
}

static void fillValidWeatherDoc(JsonDocument& doc) {
    doc["type"]       = "WeatherStation";
    doc["inst"]       = 0;
    doc["intervalMs"] = 2500;
}

// ─── Tests ───────────────────────────────────────────────────────────────────

void test_weather_station_role_type_string() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    TEST_ASSERT_EQUAL_STRING("WeatherStation", role.type());
}

void test_weather_station_role_validate_accepts_valid_config() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    role.configure(makeWeatherConfig());
    TEST_ASSERT_TRUE(role.validate());
}

void test_weather_station_role_validate_rejects_zero_interval() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    auto cfg = makeWeatherConfig();
    cfg.intervalMs = 0;
    role.configure(cfg);
    TEST_ASSERT_FALSE(role.validate());
}

void test_weather_station_role_validate_rejects_too_fast_interval() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    auto cfg = makeWeatherConfig();
    cfg.intervalMs = 99;  // below 100ms minimum
    role.configure(cfg);
    TEST_ASSERT_FALSE(role.validate());
}

void test_weather_station_role_validate_accepts_minimum_interval() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    auto cfg = makeWeatherConfig();
    cfg.intervalMs = 100;  // exact boundary
    role.configure(cfg);
    TEST_ASSERT_TRUE(role.validate());
}

void test_weather_station_role_configure_from_json_accepts_valid() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    StaticJsonDocument<128> doc;
    fillValidWeatherDoc(doc);

    TEST_ASSERT_TRUE(role.configureFromJson(doc));
}

void test_weather_station_role_configure_from_json_parses_all_fields() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    StaticJsonDocument<128> doc;
    doc["inst"]       = 3;
    doc["intervalMs"] = 5000;

    TEST_ASSERT_TRUE(role.configureFromJson(doc));
    TEST_ASSERT_EQUAL(3,    role.config.inst);
    TEST_ASSERT_EQUAL(5000, role.config.intervalMs);
}

void test_weather_station_role_configure_from_json_rejects_invalid_interval() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    StaticJsonDocument<128> doc;
    fillValidWeatherDoc(doc);
    doc["intervalMs"] = 50;  // too fast

    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}

void test_weather_station_role_json_round_trip() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    WeatherStationConfig cfg{/*inst=*/2, /*intervalMs=*/5000};
    role.configure(cfg);

    StaticJsonDocument<128> doc;
    role.getConfigJson(doc);

    TEST_ASSERT_EQUAL_STRING("WeatherStation", doc["type"].as<const char*>());
    TEST_ASSERT_EQUAL(2,    doc["inst"].as<int>());
    TEST_ASSERT_EQUAL(5000, doc["intervalMs"].as<int>());
}

void test_weather_station_role_start_sets_running() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    role.configure(makeWeatherConfig());
    role.start();

    TEST_ASSERT_TRUE(role.status().running);
}

void test_weather_station_role_start_with_invalid_config_does_not_set_running() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    auto cfg = makeWeatherConfig();
    cfg.intervalMs = 0;  // invalid
    role.configure(cfg);
    role.start();

    TEST_ASSERT_FALSE(role.status().running);
}

void test_weather_station_role_stop_clears_running() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    role.configure(makeWeatherConfig());
    role.start();
    role.stop();

    TEST_ASSERT_FALSE(role.status().running);
    TEST_ASSERT_FALSE(role.status().reason.empty());
}

void test_weather_station_role_start_clears_reason() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    role.configure(makeWeatherConfig());
    role.start();

    TEST_ASSERT_TRUE(role.status().running);
    TEST_ASSERT_TRUE(role.status().reason.empty());
}

void test_weather_station_role_loop_broadcasts_environmental_data() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {25.3f, 62.1f, 1013.25f, true};

    role.configure(makeWeatherConfig());
    role.start();

    // Advance time past one interval
    platform.setMillis(3000);
    role.loop();

    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_INT((int)MetricType::Environmental, (int)nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.3f,    nmea.lastMetric.context.environmental.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.1f,    nmea.lastMetric.context.environmental.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, nmea.lastMetric.context.environmental.pressure);
    TEST_ASSERT_EQUAL(0, nmea.lastMetric.context.environmental.inst);
}

void test_weather_station_role_loop_passes_instance_number() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {20.0f, 50.0f, 1000.0f, true};

    WeatherStationConfig cfg{/*inst=*/3, /*intervalMs=*/2500};
    role.configure(cfg);
    role.start();

    platform.setMillis(3000);
    role.loop();

    TEST_ASSERT_EQUAL(3, nmea.lastMetric.context.environmental.inst);
}

void test_weather_station_role_loop_does_not_broadcast_before_interval() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {25.0f, 60.0f, 1013.0f, true};

    role.configure(makeWeatherConfig());  // intervalMs = 2500
    role.start();                          // lastBroadcastMs_ = 0

    platform.setMillis(2499);             // just before interval
    role.loop();

    TEST_ASSERT_FALSE(nmea.sent);
}

void test_weather_station_role_loop_broadcasts_after_interval_elapses() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {25.0f, 60.0f, 1013.0f, true};

    role.configure(makeWeatherConfig());  // intervalMs = 2500
    role.start();                          // lastBroadcastMs_ = 0

    platform.setMillis(2500);             // exactly at interval
    role.loop();

    TEST_ASSERT_TRUE(nmea.sent);
}

void test_weather_station_role_loop_skips_invalid_reading() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {0.0f, 0.0f, 0.0f, false};  // invalid

    role.configure(makeWeatherConfig());
    role.start();

    platform.setMillis(3000);
    role.loop();

    TEST_ASSERT_FALSE(nmea.sent);
}

void test_weather_station_role_loop_does_not_run_when_stopped() {
    MockEnvironmentalSensorService sensor;
    FakeNmea2000Service nmea;
    MockPlatform platform;
    WeatherStationRole role(sensor, nmea, platform);

    sensor.reading = {25.0f, 60.0f, 1013.0f, true};

    role.configure(makeWeatherConfig());
    // Note: start() is NOT called

    platform.setMillis(5000);
    role.loop();

    TEST_ASSERT_FALSE(nmea.sent);
}
