#pragma once
#include <ArduinoJson.h>
#include <unity.h>

#include "FileSystem.h"
#include "MockFileSystem.h"
#include "Role.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "test_fluid_level_sensor_role.h"  // For FakeAnalogInput, FakeNmea2000Service

// Tests that RoleManager can load a valid JSON config
void test_role_manager_loads_valid_config() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    bool loaded = manager.loadRoleFromJson(json);
    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that RoleManager rejects unknown role types
void test_role_manager_rejects_unknown_type() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = R"({"type": "UnknownRole"})";

    bool loaded = manager.loadRoleFromJson(json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that RoleManager rejects invalid JSON
void test_role_manager_rejects_invalid_json() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = "{ invalid json }";

    bool loaded = manager.loadRoleFromJson(json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that startAll starts all loaded roles
void test_role_manager_starts_all_roles() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json1 = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";
    const char* json2 = R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minVoltage": 0.2,
        "maxVoltage": 4.8
    })";

    manager.loadRoleFromJson(json1);
    manager.loadRoleFromJson(json2);
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    // Should not throw when starting multiple roles
    manager.startAll();
}

// Tests that loopAll calls loop on all roles
void test_role_manager_loops_all_roles() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 1.0,
        "maxVoltage": 5.0
    })";

    manager.loadRoleFromJson(json);
    manager.startAll();

    analog.voltage = 3.0f;  // 50%
    manager.loopAll();

    // Verify the role processed data
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, nmea.lastMetric.value);
}

// Tests that loadRole loads from filesystem
void test_role_manager_loads_from_file() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    fs.addFile("/roles/FluidLevel-atd.json", R"({
        "type": "FluidLevel",
        "fluidType": "BlackWater",
        "inst": 2,
        "capacity": 50,
        "minVoltage": 0.1,
        "maxVoltage": 3.3
    })");

    bool loaded = manager.loadRole("/roles/FluidLevel-atd.json");
    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that loadFromDirectory loads all files
void test_role_manager_loads_from_directory() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    fs.addFile("/roles/water.json", R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })");
    fs.addFile("/roles/fuel.json", R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minVoltage": 0.2,
        "maxVoltage": 4.8
    })");
    fs.addDirectory("/roles", {"/roles/water.json", "/roles/fuel.json"});

    manager.loadFromDirectory("/roles");
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that getRoleInfo returns empty when no roles loaded
void test_role_manager_get_role_info_empty() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    auto info = manager.getRoleInfo();
    TEST_ASSERT_EQUAL(0, info.size());
}

// Tests that getRoleInfo returns role info with running=false before start
void test_role_manager_get_role_info_not_started() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    manager.loadRoleFromJson(json);

    auto info = manager.getRoleInfo();
    TEST_ASSERT_EQUAL(1, info.size());
    TEST_ASSERT_EQUAL_STRING("FluidLevel", info[0].id);
    TEST_ASSERT_FALSE(info[0].running);
}

// Tests that getRoleInfo shows running=true after startAll
void test_role_manager_get_role_info_after_start() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    manager.loadRoleFromJson(json);
    manager.startAll();

    auto info = manager.getRoleInfo();
    TEST_ASSERT_EQUAL(1, info.size());
    TEST_ASSERT_EQUAL_STRING("FluidLevel", info[0].id);
    TEST_ASSERT_TRUE(info[0].running);
}

// Tests that getRoleInfo works with multiple roles
void test_role_manager_get_role_info_multiple_roles() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* json1 = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";
    const char* json2 = R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minVoltage": 0.2,
        "maxVoltage": 4.8
    })";

    manager.loadRoleFromJson(json1);
    manager.loadRoleFromJson(json2);
    manager.startAll();

    auto info = manager.getRoleInfo();
    TEST_ASSERT_EQUAL(2, info.size());
    TEST_ASSERT_TRUE(info[0].running);
    TEST_ASSERT_TRUE(info[1].running);
}

// Tests that getRoleConfigsJson returns empty object when no roles
void test_role_manager_get_configs_json_empty() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    std::string json = manager.getRoleConfigsJson();
    TEST_ASSERT_EQUAL_STRING("{}", json.c_str());
}

// Tests that getRoleConfigsJson returns config for single role
void test_role_manager_get_configs_json_single_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    manager.loadRoleFromJson(roleJson);

    std::string json = manager.getRoleConfigsJson();

    // Parse and verify
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_TRUE(doc.containsKey("FluidLevel"));
    JsonObject config = doc["FluidLevel"];
    TEST_ASSERT_EQUAL_STRING("FluidLevel", config["type"]);
    TEST_ASSERT_EQUAL_STRING("Water", config["fluidType"]);
    TEST_ASSERT_EQUAL(1, config["inst"]);
    TEST_ASSERT_EQUAL(100, config["capacity"]);
}

// Tests that updateRoleConfig updates existing role
void test_role_manager_update_role_config() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    manager.loadRoleFromJson(roleJson);

    // Update config
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Fuel";
    updateDoc["inst"] = 2;
    updateDoc["capacity"] = 200;
    updateDoc["minVoltage"] = 0.2;
    updateDoc["maxVoltage"] = 4.8;

    bool result = manager.updateRoleConfig("FluidLevel", updateDoc);
    TEST_ASSERT_TRUE(result);

    // Verify update via getRoleConfigsJson
    std::string json = manager.getRoleConfigsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject config = doc["FluidLevel"];
    TEST_ASSERT_EQUAL_STRING("Fuel", config["fluidType"]);
    TEST_ASSERT_EQUAL(2, config["inst"]);
    TEST_ASSERT_EQUAL(200, config["capacity"]);
}

// Tests that updateRoleConfig returns false for unknown role
void test_role_manager_update_role_config_unknown_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Water";
    updateDoc["minVoltage"] = 0.5;
    updateDoc["maxVoltage"] = 4.5;

    bool result = manager.updateRoleConfig("NonExistentRole", updateDoc);
    TEST_ASSERT_FALSE(result);
}

// Tests that updateRoleConfig persists to filesystem
void test_role_manager_update_role_config_persists() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    // Load from file so ID matches filename
    fs.addFile("/roles/FluidLevel-abc.json", R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })");

    manager.loadRole("/roles/FluidLevel-abc.json");

    // Update config
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Fuel";
    updateDoc["inst"] = 2;
    updateDoc["capacity"] = 200;
    updateDoc["minVoltage"] = 0.2;
    updateDoc["maxVoltage"] = 4.8;

    bool result = manager.updateRoleConfig("FluidLevel-abc", updateDoc);
    TEST_ASSERT_TRUE(result);

    // Trigger deferred persist via loopAll
    manager.loopAll();

    // Verify file was written
    const std::string* written = fs.getWrittenFile("/roles/FluidLevel-abc.json");
    TEST_ASSERT_NOT_NULL(written);

    // Parse and verify content
    StaticJsonDocument<256> savedDoc;
    deserializeJson(savedDoc, *written);
    TEST_ASSERT_EQUAL_STRING("Fuel", savedDoc["fluidType"]);
    TEST_ASSERT_EQUAL(2, savedDoc["inst"]);
    TEST_ASSERT_EQUAL(200, savedDoc["capacity"]);
}

// Tests that updateRoleConfig returns false for invalid config
void test_role_manager_update_role_config_invalid() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    manager.loadRoleFromJson(roleJson);

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Water";
    updateDoc["inst"] = 0;
    updateDoc["capacity"] = 100;
    updateDoc["minVoltage"] = 5.0;
    updateDoc["maxVoltage"] = 1.0;

    bool result = manager.updateRoleConfig("FluidLevel", updateDoc);
    TEST_ASSERT_FALSE(result);
}
