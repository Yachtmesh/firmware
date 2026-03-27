#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include "MockSerialSensorService.h"
#include "VeDirectBatteryRole.h"
#include "VeDirectParser.h"
#include "test_fluid_level_sensor_role.h"  // FakeNmea2000Service

// ── Checksum helper ──────────────────────────────────────────────────────────
//
// Feed a complete, checksum-correct VE.Direct frame into a parser:
//   V=12000 mV, I=2000 mA, SOC=900 ‰, TTG=480 min, CE=-5200 mAh
//   Checksum byte = '4' (0x34) — verified by summing all bytes mod 256 = 0.
static void feedFullFrame(VeDirectParser& p) {
    p.feedLine("V",   "12000");
    p.feedLine("I",   "2000");
    p.feedLine("SOC", "900");
    p.feedLine("TTG", "480");
    p.feedLine("CE",  "-5200");
    p.feedLine("Checksum", "4");
}

// Enqueue the same frame into a MockSerialSensorService as tab-delimited lines.
static void enqueueFullFrame(MockSerialSensorService& serial) {
    serial.enqueue("V\t12000");
    serial.enqueue("I\t2000");
    serial.enqueue("SOC\t900");
    serial.enqueue("TTG\t480");
    serial.enqueue("CE\t-5200");
    serial.enqueue("Checksum\t4");
}

// ── VeDirectParser tests ─────────────────────────────────────────────────────

void test_ve_direct_parser_complete_frame_checksums_ok() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_TRUE(parser.getFrame().checksumOk);
}

void test_ve_direct_parser_voltage() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, parser.getFrame().voltage);
}

void test_ve_direct_parser_current() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, parser.getFrame().current);
}

void test_ve_direct_parser_soc() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, parser.getFrame().soc);
}

void test_ve_direct_parser_ttg() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 480.0f, parser.getFrame().ttg);
}

void test_ve_direct_parser_consumed_ah() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.2f, parser.getFrame().consumedAh);
}

void test_ve_direct_parser_returns_true_on_checksum_line() {
    VeDirectParser parser;
    TEST_ASSERT_FALSE(parser.feedLine("V", "12000"));
    TEST_ASSERT_FALSE(parser.feedLine("I", "2000"));
    TEST_ASSERT_FALSE(parser.feedLine("SOC", "900"));
    TEST_ASSERT_FALSE(parser.feedLine("TTG", "480"));
    TEST_ASSERT_FALSE(parser.feedLine("CE", "-5200"));
    TEST_ASSERT_TRUE(parser.feedLine("Checksum", "4"));
}

void test_ve_direct_parser_bad_checksum_marked_invalid() {
    VeDirectParser parser;
    parser.feedLine("V",   "12000");
    parser.feedLine("I",   "2000");
    parser.feedLine("SOC", "900");
    parser.feedLine("Checksum", "X");  // wrong byte
    TEST_ASSERT_FALSE(parser.getFrame().checksumOk);
}

void test_ve_direct_parser_negative_ttg_becomes_unavailable() {
    VeDirectParser parser;
    parser.feedLine("TTG", "-1");
    // TTG=-1 means "calculating" in VE.Direct — should map to -1 (unavailable)
    parser.feedLine("Checksum", "z");  // will fail checksum but TTG is already set
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, parser.getFrame().ttg);
}

void test_ve_direct_parser_reset_clears_state() {
    VeDirectParser parser;
    feedFullFrame(parser);
    TEST_ASSERT_TRUE(parser.getFrame().checksumOk);

    parser.reset();
    // After reset, a bad checksum line should fail
    parser.feedLine("Checksum", "X");
    TEST_ASSERT_FALSE(parser.getFrame().checksumOk);
}

void test_ve_direct_parser_checksum_only_frame_has_no_data() {
    // Regression: after a parser reset, if only the Checksum line arrives
    // (data fields lost to UART overflow/desync), hasData must be false even
    // if the byte value makes the running sum equal 0 (checksumOk may be true).
    // "\xad" is the byte that makes "Checksum\t<byte>\r\n" alone sum to 0 mod 256.
    VeDirectParser parser;
    parser.feedLine("Checksum", "\xad");
    TEST_ASSERT_FALSE(parser.getFrame().hasData);
}

void test_ve_direct_parser_newline_checksum_byte_accepted() {
    // Regression: when the VE.Direct checksum byte is '\n' (0x0A), readLine()
    // consumes it as a line terminator, leaving an empty val for the Checksum
    // field. The role must detect this and pass '\n' to feedLine instead.
    //
    // "V\t940000\r\n" bytes sum to 419.
    // "Checksum\t\n\r\n" bytes sum to 861.
    // 419 + 861 = 1280 = 5*256 → valid checksum.
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    serial.enqueue("V\t940000");  // 940 V in mV — data field with known byte sum
    serial.enqueue("Checksum\t"); // empty val: readLine() consumed '\n' as line end

    role.loop();

    TEST_ASSERT_EQUAL(MetricType::BatteryStatus, nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 940.0f, nmea.lastMetric.context.battery.voltage);
}

// ── VeDirectBatteryRole tests ────────────────────────────────────────────────

void test_ve_direct_battery_role_type_string() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    TEST_ASSERT_EQUAL_STRING("VeDirectBattery", role.type());
}

void test_ve_direct_battery_role_validate_always_true() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    TEST_ASSERT_TRUE(role.validate());
}

void test_ve_direct_battery_role_start_sets_running() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    TEST_ASSERT_TRUE(role.status().running);
    TEST_ASSERT_TRUE(role.status().reason.empty());
}

void test_ve_direct_battery_role_stop_clears_running() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    role.stop();
    TEST_ASSERT_FALSE(role.status().running);
    TEST_ASSERT_FALSE(role.status().reason.empty());
}

void test_ve_direct_battery_role_loop_sends_metric_on_complete_frame() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();

    enqueueFullFrame(serial);
    role.loop();

    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL(MetricType::BatteryStatus, nmea.lastMetric.type);
}

void test_ve_direct_battery_role_loop_correct_voltage() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    enqueueFullFrame(serial);
    role.loop();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, nmea.lastMetric.context.battery.voltage);
}

void test_ve_direct_battery_role_loop_correct_current() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    enqueueFullFrame(serial);
    role.loop();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, nmea.lastMetric.context.battery.current);
}

void test_ve_direct_battery_role_loop_correct_soc() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    enqueueFullFrame(serial);
    role.loop();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, nmea.lastMetric.context.battery.soc);
}

void test_ve_direct_battery_role_loop_correct_ttg() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    enqueueFullFrame(serial);
    role.loop();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 480.0f, nmea.lastMetric.context.battery.ttg);
}

void test_ve_direct_battery_role_loop_correct_instance() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    VeDirectBatteryConfig cfg(/*inst=*/1);
    role.configure(cfg);
    role.start();
    enqueueFullFrame(serial);
    role.loop();
    TEST_ASSERT_EQUAL(1, nmea.lastMetric.context.battery.inst);
}

void test_ve_direct_battery_role_loop_bad_checksum_not_sent() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();

    serial.enqueue("V\t12000");
    serial.enqueue("I\t2000");
    serial.enqueue("Checksum\tX");  // bad checksum byte
    role.loop();

    TEST_ASSERT_FALSE(nmea.sent);
}

void test_ve_direct_battery_role_loop_no_data_no_send() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);
    role.start();
    role.loop();  // nothing enqueued
    TEST_ASSERT_FALSE(nmea.sent);
}

void test_ve_direct_battery_role_configure_from_json() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);

    StaticJsonDocument<128> doc;
    doc["inst"]  = 2;
    doc["rxPin"] = 16;
    doc["txPin"] = 17;

    TEST_ASSERT_TRUE(role.configureFromJson(doc));
    TEST_ASSERT_EQUAL(2,  role.config.inst);
    TEST_ASSERT_EQUAL(16, role.config.rxPin);
    TEST_ASSERT_EQUAL(17, role.config.txPin);
}

void test_ve_direct_battery_role_get_config_json() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);

    VeDirectBatteryConfig cfg(/*inst=*/1, /*rx=*/16, /*tx=*/17);
    role.configure(cfg);

    StaticJsonDocument<128> doc;
    role.getConfigJson(doc);

    TEST_ASSERT_EQUAL_STRING("VeDirectBattery", doc["type"]);
    TEST_ASSERT_EQUAL(1,  doc["inst"]);
    TEST_ASSERT_EQUAL(16, doc["rxPin"]);
    TEST_ASSERT_EQUAL(17, doc["txPin"]);
}

void test_ve_direct_battery_role_config_json_roundtrip() {
    FakeNmea2000Service nmea;
    MockSerialSensorService serial;
    VeDirectBatteryRole role(nmea, serial);

    StaticJsonDocument<128> doc;
    doc["inst"]  = 3;
    doc["rxPin"] = 16;
    doc["txPin"] = 17;
    role.configureFromJson(doc);

    StaticJsonDocument<128> out;
    role.getConfigJson(out);

    TEST_ASSERT_EQUAL(3,  out["inst"]);
    TEST_ASSERT_EQUAL(16, out["rxPin"]);
    TEST_ASSERT_EQUAL(17, out["txPin"]);
}
