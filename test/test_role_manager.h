#pragma once
#include <ArduinoJson.h>
#include <unity.h>

#include "FileSystem.h"
#include "MockCurrentSensorManager.h"
#include "MockEnvironmentalSensorService.h"
#include "MockFileSystem.h"
#include "MockPlatform.h"
#include "MockSerialSensorService.h"
#include "Role.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "test_fluid_level_sensor_role.h"  // For FakeNmea2000Service
#include "test_wifi_gateway_role.h"        // For FakeWifiService

// Shared fixtures for role manager tests.
static MockEnvironmentalSensorService gTestEnvSensor;
static MockSerialSensorService gTestSerialSensor;

// Test helper: parse JSON string (flat format) and load role (no persist)
// Converts flat config to hierarchical format: {roleId, roleType, config}
// Returns true if parsing and loading succeeded
inline bool loadRoleFromJsonString(RoleManager& manager, const char* json,
                                   const char* instanceId = nullptr) {
    StaticJsonDocument<512> flatDoc;
    if (deserializeJson(flatDoc, json)) {
        return false;
    }
    const char* type = flatDoc["type"] | "";
    std::string id = instanceId ? instanceId : (std::string(type) + "-test");

    // Convert to hierarchical format
    StaticJsonDocument<768> doc;
    doc["roleId"].set(id);       // Force copy with .set()
    doc["roleType"].set(type);   // Force copy with .set()
    JsonObject config = doc.createNestedObject("config");
    for (JsonPairConst kv : flatDoc.as<JsonObjectConst>()) {
        config[kv.key().c_str()].set(kv.value());
    }

    return manager.applyRoleConfig(doc, false).success;
}

// Tests that RoleManager can load a valid JSON config
void test_role_manager_loads_valid_config() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that RoleManager rejects unknown role types
void test_role_manager_rejects_unknown_type() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json = R"({"type": "UnknownRole"})";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that RoleManager rejects invalid JSON
void test_role_manager_rejects_invalid_json() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json = "{ invalid json }";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that startAll starts all loaded roles
void test_role_manager_starts_all_roles() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json1 = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";
    const char* json2 = R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minCurrent": 0.002,
        "maxCurrent": 0.018,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, json1, "FluidLevel-001");
    loadRoleFromJsonString(manager, json2, "FluidLevel-002");
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    // Should not throw when starting multiple roles
    manager.startAll();
}

// Tests that loopAll calls loop on all roles
void test_role_manager_loops_all_roles() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 1.0,
        "maxCurrent": 5.0,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, json);
    manager.startAll();

    currentSensorManager.sensor.reading.current = 3.0f;  // 50% with min=1.0, max=5.0
    manager.loopAll();

    // Verify the role processed data
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, nmea.lastMetric.value);
}

// Tests that loadRolesFromDirectory loads from filesystem
void test_role_manager_loads_from_file() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    fs.addFile("/roles/FluidLevel-atd.json", R"({
        "roleId": "FluidLevel-atd",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "BlackWater",
            "inst": 2,
            "capacity": 50,
            "minCurrent": 0.1,
            "maxCurrent": 3.3
        }
    })");
    fs.addDirectory("/roles", {"/roles/FluidLevel-atd.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that loadRolesFromDirectory loads all files
void test_role_manager_loads_from_directory() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    fs.addFile("/roles/water.json", R"({
        "roleId": "water",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Water",
            "inst": 0,
            "capacity": 100,
            "minCurrent": 0.5,
            "maxCurrent": 4.5
        }
    })");
    fs.addFile("/roles/fuel.json", R"({
        "roleId": "fuel",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Fuel",
            "inst": 1,
            "capacity": 200,
            "minCurrent": 0.2,
            "maxCurrent": 4.8
        }
    })");
    fs.addDirectory("/roles", {"/roles/water.json", "/roles/fuel.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that getRolesAsJson returns empty array when no roles loaded
void test_role_manager_get_roles_as_json_empty() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    std::string json = manager.getRolesAsJson();
    TEST_ASSERT_EQUAL_STRING("[]", json.c_str());
}

// Tests that getRolesAsJson returns role with running=false before start
void test_role_manager_get_roles_as_json_not_started() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-y01");

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_TRUE(doc.is<JsonArray>());
    TEST_ASSERT_EQUAL(1, doc.size());

    JsonObject role = doc[0];
    TEST_ASSERT_EQUAL_STRING("FluidLevel-y01", role["id"]);
    TEST_ASSERT_EQUAL_STRING("FluidLevel", role["type"]);
    TEST_ASSERT_FALSE(role["status"]["running"]);
}

// Tests that getRolesAsJson shows running=true after startAll
void test_role_manager_get_roles_as_json_after_start() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson);
    manager.startAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject role = doc[0];
    TEST_ASSERT_TRUE(role["status"]["running"]);
}

// Tests that getRolesAsJson works with multiple roles
void test_role_manager_get_roles_as_json_multiple() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json1 = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";
    const char* json2 = R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minCurrent": 0.002,
        "maxCurrent": 0.018,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, json1, "FluidLevel-abc");
    loadRoleFromJsonString(manager, json2, "FluidLevel-xyz");
    manager.startAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_EQUAL(2, doc.size());
    TEST_ASSERT_TRUE(doc[0]["status"]["running"]);
    TEST_ASSERT_TRUE(doc[1]["status"]["running"]);
}

// Tests that getRolesAsJson includes ipAddress in WifiGateway role status when connected
void test_role_manager_get_roles_as_json_includes_ip_address() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "WifiGateway",
        "ssid": "BoatNet",
        "password": "sail123",
        "port": 10110
    })";

    loadRoleFromJsonString(manager, roleJson, "WifiGateway-abc");
    manager.startAll();

    strncpy(wifi.ip, "192.168.1.42", sizeof(wifi.ip) - 1);
    manager.loopAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_EQUAL(1, doc.size());
    JsonObject role = doc[0];
    TEST_ASSERT_EQUAL_STRING("192.168.1.42", role["status"]["ip"] | "");
}

// Tests that ip address assigned after startup propagates to JSON on next loopAll
void test_role_manager_ip_address_updates_after_wifi_connects() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "WifiGateway",
        "ssid": "BoatNet",
        "password": "sail123",
        "port": 10110
    })";

    loadRoleFromJsonString(manager, roleJson, "WifiGateway-abc");
    manager.startAll();

    // First loop — WiFi not yet connected, IP is empty string
    manager.loopAll();
    std::string json1 = manager.getRolesAsJson();
    StaticJsonDocument<1024> doc1;
    deserializeJson(doc1, json1);
    TEST_ASSERT_EQUAL_STRING("", doc1[0]["status"]["ip"] | "");

    // WiFi connects and DHCP assigns an IP
    strncpy(wifi.ip, "10.0.0.5", sizeof(wifi.ip) - 1);

    // Second loop — should pick up the new IP
    manager.loopAll();
    std::string json2 = manager.getRolesAsJson();
    StaticJsonDocument<1024> doc2;
    deserializeJson(doc2, json2);
    TEST_ASSERT_EQUAL_STRING("10.0.0.5", doc2[0]["status"]["ip"] | "");
}

// Tests that getRolesAsJson omits ipAddress for roles that don't have one
void test_role_manager_get_roles_as_json_no_ip_for_other_roles() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");
    manager.startAll();
    manager.loopAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject role = doc[0];
    TEST_ASSERT_FALSE(role["status"].containsKey("ip"));
}

// Tests that getRolesAsJson includes config with all fields
void test_role_manager_get_roles_as_json_config_fields() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-cfg");

    std::string configJson = manager.getRoleConfigJson("FluidLevel-cfg");
    StaticJsonDocument<512> doc;
    deserializeJson(doc, configJson);

    TEST_ASSERT_TRUE(doc.containsKey("config"));

    JsonObject config = doc["config"];
    TEST_ASSERT_EQUAL_STRING("FluidLevel", config["type"]);
    TEST_ASSERT_EQUAL_STRING("Water", config["fluidType"]);
    TEST_ASSERT_EQUAL(1, config["inst"]);
    TEST_ASSERT_EQUAL(100, config["capacity"]);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.005, config["minCurrent"]);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.02, config["maxCurrent"]);
}

// Tests that applyRoleConfig updates existing role
void test_role_manager_update_role_config() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config using hierarchical format
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 2;
    config["capacity"] = 200;
    config["minCurrent"] = 0.2;
    config["maxCurrent"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Update succeeded");

    // Verify update via getRoleConfigJson
    std::string configJson = manager.getRoleConfigJson("FluidLevel-abc");
    StaticJsonDocument<512> doc;
    deserializeJson(doc, configJson);

    JsonObject roleConfig = doc["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", roleConfig["fluidType"]);
    TEST_ASSERT_EQUAL(2, roleConfig["inst"]);
    TEST_ASSERT_EQUAL(200, roleConfig["capacity"]);
}

// Tests that applyRoleConfig with unknown roleId creates new role (if roleType provided)
void test_role_manager_update_role_config_unknown_role() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Without roleType, should fail for unknown roleId
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "NonExistentRole";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    // Now creates a new role since roleId not found but falls through to create
    // However, without roleType it should fail
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing roleType for new role", result.error.c_str());
}

// Tests that applyRoleConfig persists updates to filesystem
void test_role_manager_update_role_config_persists() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Load role with specific ID (no persist)
    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config using hierarchical format (with persist)
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 2;
    config["capacity"] = 200;
    config["minCurrent"] = 0.2;
    config["maxCurrent"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    TEST_ASSERT_TRUE(result.success);

    // Trigger deferred persist via loopAll
    manager.loopAll();

    // Verify file was written
    const std::string* written =
        fs.getWrittenFile("/roles/FluidLevel-abc.json");
    TEST_ASSERT_NOT_NULL(written);

    // Parse and verify content - now hierarchical format
    StaticJsonDocument<512> savedDoc;
    deserializeJson(savedDoc, *written);
    TEST_ASSERT_EQUAL_STRING("FluidLevel-abc", savedDoc["roleId"]);
    TEST_ASSERT_EQUAL_STRING("FluidLevel", savedDoc["roleType"]);
    JsonObject savedConfig = savedDoc["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", savedConfig["fluidType"]);
    TEST_ASSERT_EQUAL(2, savedConfig["inst"]);
    TEST_ASSERT_EQUAL(200, savedConfig["capacity"]);
}

// Tests that applyRoleConfig returns error for invalid config
void test_role_manager_update_role_config_invalid() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Invalid: minCurrent >= maxCurrent
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 5.0;
    config["maxCurrent"] = 1.0;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    TEST_ASSERT_FALSE(result.success);
}

// Tests that applyRoleConfig creates a new role and returns generated ID
void test_role_manager_create_role() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());
    TEST_ASSERT_EQUAL(1, manager.roleCount());

    // Verify ID starts with type
    TEST_ASSERT_TRUE(result.roleId.rfind("FluidLevel-", 0) == 0);
}

// Tests that applyRoleConfig fails for unknown type
void test_role_manager_create_role_unknown_type() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "UnknownRole";
    doc.createNestedObject("config");

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig fails for missing/empty type
void test_role_manager_create_role_missing_type() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "";  // Empty type
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig fails for invalid config
void test_role_manager_create_role_invalid_config() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Invalid: minCurrent >= maxCurrent
    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 5.0;
    config["maxCurrent"] = 1.0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig persists the new role to filesystem
void test_role_manager_create_role_persists() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 1;
    config["capacity"] = 200;
    config["minCurrent"] = 0.2;
    config["maxCurrent"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());

    // Trigger deferred persist via loopAll
    manager.loopAll();

    // Verify file was written
    std::string path = "/roles/" + result.roleId + ".json";
    const std::string* written = fs.getWrittenFile(path);
    TEST_ASSERT_NOT_NULL(written);

    // Parse and verify content - now hierarchical format
    StaticJsonDocument<512> savedDoc;
    deserializeJson(savedDoc, *written);
    TEST_ASSERT_EQUAL_STRING("FluidLevel", savedDoc["roleType"]);
    TEST_ASSERT_TRUE(std::string(savedDoc["roleId"]).rfind("FluidLevel-", 0) == 0);
    JsonObject savedConfig = savedDoc["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", savedConfig["fluidType"]);
    TEST_ASSERT_EQUAL(1, savedConfig["inst"]);
    TEST_ASSERT_EQUAL(200, savedConfig["capacity"]);
}

// Tests that new roles are not started immediately in applyRoleConfig (BLE
// callback safety) but are started after loopAll
void test_role_manager_new_role_start_deferred_to_loop() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "WifiGateway";
    JsonObject config = doc.createNestedObject("config");
    config["ssid"] = "TestNet";
    config["password"] = "pass";
    config["port"] = 10110;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);

    // Should NOT be running immediately after applyRoleConfig
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<1024> roles;
    deserializeJson(roles, json);
    TEST_ASSERT_FALSE(roles[0]["status"]["running"]);

    // Should be running after loopAll processes the deferred start
    manager.loopAll();
    json = manager.getRolesAsJson();
    deserializeJson(roles, json);
    TEST_ASSERT_TRUE(roles[0]["status"]["running"]);
}

// Tests that applyRoleConfig generates unique IDs for multiple roles
void test_role_manager_create_role_unique_ids() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc1;
    doc1["roleType"] = "FluidLevel";
    JsonObject config1 = doc1.createNestedObject("config");
    config1["fluidType"] = "Water";
    config1["inst"] = 0;
    config1["capacity"] = 100;
    config1["minCurrent"] = 0.5;
    config1["maxCurrent"] = 4.5;

    StaticJsonDocument<512> doc2;
    doc2["roleType"] = "FluidLevel";
    JsonObject config2 = doc2.createNestedObject("config");
    config2["fluidType"] = "Fuel";
    config2["inst"] = 1;
    config2["capacity"] = 200;
    config2["minCurrent"] = 0.2;
    config2["maxCurrent"] = 4.8;

    ApplyConfigResult result1 = manager.applyRoleConfig(doc1);
    ApplyConfigResult result2 = manager.applyRoleConfig(doc2);

    TEST_ASSERT_TRUE(result1.success);
    TEST_ASSERT_TRUE(result2.success);
    TEST_ASSERT_FALSE(result1.roleId.empty());
    TEST_ASSERT_FALSE(result2.roleId.empty());
    TEST_ASSERT_TRUE(result1.roleId != result2.roleId);
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that factoryReset clears all roles
void test_role_manager_factory_reset_clears_roles() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    const char* json1 = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";
    const char* json2 = R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minCurrent": 0.002,
        "maxCurrent": 0.018,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";

    loadRoleFromJsonString(manager, json1, "FluidLevel-001");
    loadRoleFromJsonString(manager, json2, "FluidLevel-002");
    manager.startAll();
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    // Trigger factory reset
    manager.factoryReset();
    manager.loopAll();

    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that factoryReset deletes config files
void test_role_manager_factory_reset_deletes_files() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Set up files and directory with hierarchical format
    fs.addFile("/roles/FluidLevel-abc.json", R"({
        "roleId": "FluidLevel-abc",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Water",
            "inst": 0,
            "capacity": 100,
            "minCurrent": 0.5,
            "maxCurrent": 4.5
        }
    })");
    fs.addFile("/roles/FluidLevel-xyz.json", R"({
        "roleId": "FluidLevel-xyz",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Fuel",
            "inst": 1,
            "capacity": 200,
            "minCurrent": 0.2,
            "maxCurrent": 4.8
        }
    })");
    fs.addDirectory(
        "/roles", {"/roles/FluidLevel-abc.json", "/roles/FluidLevel-xyz.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    // Trigger factory reset
    manager.factoryReset();
    manager.loopAll();

    // Verify files were removed
    TEST_ASSERT_TRUE(fs.wasRemoved("/roles/FluidLevel-abc.json"));
    TEST_ASSERT_TRUE(fs.wasRemoved("/roles/FluidLevel-xyz.json"));
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that factoryReset works with empty roles directory
void test_role_manager_factory_reset_empty() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    fs.addDirectory("/roles", {});

    // Should not crash with no roles
    manager.factoryReset();
    manager.loopAll();

    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that factoryReset clears pending persists
void test_role_manager_factory_reset_clears_pending() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Create a role (adds to pending persist)
    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());

    // Set up directory for reset
    fs.addDirectory("/roles", {});

    // Factory reset before loopAll would persist
    manager.factoryReset();
    manager.loopAll();

    // Verify role was not persisted (factory reset cleared pending)
    std::string path = "/roles/" + result.roleId + ".json";
    const std::string* written = fs.getWrittenFile(path);
    TEST_ASSERT_NULL(written);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig creates role when no roleId provided
void test_role_manager_apply_config_creates_role() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());
    TEST_ASSERT_TRUE(result.roleId.rfind("FluidLevel-", 0) == 0);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that applyRoleConfig updates role when roleId provided
void test_role_manager_apply_config_updates_role() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // First create a role
    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minCurrent": 0.005,
        "maxCurrent": 0.02,
        "i2cAddress": 64,
        "shuntOhms": 0.1
    })";
    loadRoleFromJsonString(manager, roleJson, "FluidLevel-xyz");

    // Update via applyRoleConfig
    StaticJsonDocument<512> doc;
    doc["roleId"] = "FluidLevel-xyz";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 1;
    config["capacity"] = 200;
    config["minCurrent"] = 0.2;
    config["maxCurrent"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("FluidLevel-xyz", result.roleId.c_str());

    // Verify update via getRoleConfigJson
    std::string configJson = manager.getRoleConfigJson("FluidLevel-xyz");
    StaticJsonDocument<512> rolesDoc;
    deserializeJson(rolesDoc, configJson);

    JsonObject roleConfig = rolesDoc["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", roleConfig["fluidType"]);
    TEST_ASSERT_EQUAL(200, roleConfig["capacity"]);
}

// Tests that applyRoleConfig returns correct roleId
void test_role_manager_apply_config_returns_role_id() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    // Verify returned ID matches what's in RoleManager
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> rolesDoc;
    deserializeJson(rolesDoc, json);

    const char* actualId = rolesDoc[0]["id"];
    TEST_ASSERT_EQUAL_STRING(actualId, result.roleId.c_str());
}

// Tests that applyRoleConfig fails on missing config
void test_role_manager_apply_config_missing_config() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc;
    doc["roleType"] = "FluidLevel";
    // No config object

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL_STRING("Missing config object", result.error.c_str());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig fails on missing roleType for create
void test_role_manager_apply_config_missing_role_type() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    // No roleId (so it's a create)
    // No roleType
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL_STRING("Missing roleType for new role", result.error.c_str());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig with unknown roleId falls through to create (needs roleType)
void test_role_manager_apply_config_unknown_role() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Unknown roleId without roleType should fail
    StaticJsonDocument<512> doc;
    doc["roleId"] = "NonExistent-xyz";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing roleType for new role", result.error.c_str());
}

// Tests that removeRole is a no-op for an unknown ID
void test_role_manager_remove_role_unknown_id() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    loadRoleFromJsonString(manager, R"({
        "type": "FluidLevel", "fluidType": "Water",
        "inst": 0, "capacity": 100, "minCurrent": 0.5, "maxCurrent": 4.5
    })", "FluidLevel-abc");
    TEST_ASSERT_EQUAL(1, manager.roleCount());

    manager.removeRole("NonExistent-xyz");
    manager.loopAll();

    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that removeRole stops and removes an existing role
void test_role_manager_remove_role_removes_from_list() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    loadRoleFromJsonString(manager, R"({
        "type": "FluidLevel", "fluidType": "Water",
        "inst": 0, "capacity": 100, "minCurrent": 0.5, "maxCurrent": 4.5
    })", "FluidLevel-abc");
    loadRoleFromJsonString(manager, R"({
        "type": "FluidLevel", "fluidType": "Fuel",
        "inst": 1, "capacity": 200, "minCurrent": 0.2, "maxCurrent": 4.8
    })", "FluidLevel-xyz");
    manager.startAll();
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    manager.removeRole("FluidLevel-abc");
    manager.loopAll();

    TEST_ASSERT_EQUAL(1, manager.roleCount());

    // Verify the remaining role is the correct one
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<256> doc;
    deserializeJson(doc, json);
    TEST_ASSERT_EQUAL_STRING("FluidLevel-xyz", doc[0]["id"]);
}

// Tests that removeRole deletes the config file from the filesystem
void test_role_manager_remove_role_deletes_file() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    fs.addFile("/roles/FluidLevel-abc.json", R"({
        "roleId": "FluidLevel-abc", "roleType": "FluidLevel",
        "config": {"fluidType": "Water", "inst": 0, "capacity": 100,
                   "minCurrent": 0.5, "maxCurrent": 4.5}
    })");
    fs.addDirectory("/roles", {"/roles/FluidLevel-abc.json"});
    loadRolesFromDirectory(manager, fs, "/roles");

    manager.removeRole("FluidLevel-abc");
    manager.loopAll();

    TEST_ASSERT_TRUE(fs.wasRemoved("/roles/FluidLevel-abc.json"));
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that removeRole cancels a pending persist for that role
void test_role_manager_remove_role_clears_pending_persist() {
    MockCurrentSensorManager currentSensorManager;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    MockFileSystem fs;
    MockPlatform platform;
    RoleFactory factory(currentSensorManager, nmea, wifi, platform, gTestEnvSensor, gTestSerialSensor, fakeTcpCreator());
    RoleManager manager(factory, fs);

    // Create a role (queues a persist)
    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minCurrent"] = 0.5;
    config["maxCurrent"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);

    // Remove before loopAll can persist
    manager.removeRole(result.roleId.c_str());
    manager.loopAll();

    // File should not have been written
    std::string path = "/roles/" + result.roleId + ".json";
    TEST_ASSERT_NULL(fs.getWrittenFile(path));
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}
