#pragma once
#include <ArduinoJson.h>
#include <unity.h>

#include "FileSystem.h"
#include "MockFileSystem.h"
#include "Role.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "test_fluid_level_sensor_role.h"  // For FakeAnalogInput, FakeNmea2000Service
#include "test_wifi_gateway_role.h"        // For FakeWifiService

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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that RoleManager rejects unknown role types
void test_role_manager_rejects_unknown_type() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* json = R"({"type": "UnknownRole"})";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that RoleManager rejects invalid JSON
void test_role_manager_rejects_invalid_json() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* json = "{ invalid json }";

    bool loaded = loadRoleFromJsonString(manager, json);
    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that startAll starts all loaded roles
void test_role_manager_starts_all_roles() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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

    loadRoleFromJsonString(manager, json1, "FluidLevel-001");
    loadRoleFromJsonString(manager, json2, "FluidLevel-002");
    TEST_ASSERT_EQUAL(2, manager.roleCount());

    // Should not throw when starting multiple roles
    manager.startAll();
}

// Tests that loopAll calls loop on all roles
void test_role_manager_loops_all_roles() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* json = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 1.0,
        "maxVoltage": 5.0
    })";

    loadRoleFromJsonString(manager, json);
    manager.startAll();

    analog.voltage = 3.0f;  // 50%
    manager.loopAll();

    // Verify the role processed data
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, nmea.lastMetric.value);
}

// Tests that loadRolesFromDirectory loads from filesystem
void test_role_manager_loads_from_file() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    fs.addFile("/roles/FluidLevel-atd.json", R"({
        "roleId": "FluidLevel-atd",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "BlackWater",
            "inst": 2,
            "capacity": 50,
            "minVoltage": 0.1,
            "maxVoltage": 3.3
        }
    })");
    fs.addDirectory("/roles", {"/roles/FluidLevel-atd.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that loadRolesFromDirectory loads all files
void test_role_manager_loads_from_directory() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    fs.addFile("/roles/water.json", R"({
        "roleId": "water",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Water",
            "inst": 0,
            "capacity": 100,
            "minVoltage": 0.5,
            "maxVoltage": 4.5
        }
    })");
    fs.addFile("/roles/fuel.json", R"({
        "roleId": "fuel",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Fuel",
            "inst": 1,
            "capacity": 200,
            "minVoltage": 0.2,
            "maxVoltage": 4.8
        }
    })");
    fs.addDirectory("/roles", {"/roles/water.json", "/roles/fuel.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that getRolesAsJson returns empty array when no roles loaded
void test_role_manager_get_roles_as_json_empty() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    std::string json = manager.getRolesAsJson();
    TEST_ASSERT_EQUAL_STRING("[]", json.c_str());
}

// Tests that getRolesAsJson returns role with running=false before start
void test_role_manager_get_roles_as_json_not_started() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
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
    TEST_ASSERT_FALSE(role["running"]);
}

// Tests that getRolesAsJson shows running=true after startAll
void test_role_manager_get_roles_as_json_after_start() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson);
    manager.startAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject role = doc[0];
    TEST_ASSERT_TRUE(role["running"]);
}

// Tests that getRolesAsJson works with multiple roles
void test_role_manager_get_roles_as_json_multiple() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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

    loadRoleFromJsonString(manager, json1, "FluidLevel-abc");
    loadRoleFromJsonString(manager, json2, "FluidLevel-xyz");
    manager.startAll();

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, json);

    TEST_ASSERT_EQUAL(2, doc.size());
    TEST_ASSERT_TRUE(doc[0]["running"]);
    TEST_ASSERT_TRUE(doc[1]["running"]);
}

// Tests that getRolesAsJson includes config with all fields
void test_role_manager_get_roles_as_json_config_fields() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 1,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson);

    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject role = doc[0];
    TEST_ASSERT_TRUE(role.containsKey("config"));

    JsonObject config = role["config"];
    TEST_ASSERT_EQUAL_STRING("FluidLevel", config["type"]);
    TEST_ASSERT_EQUAL_STRING("Water", config["fluidType"]);
    TEST_ASSERT_EQUAL(1, config["inst"]);
    TEST_ASSERT_EQUAL(100, config["capacity"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5, config["minVoltage"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.5, config["maxVoltage"]);
}

// Tests that applyRoleConfig updates existing role
void test_role_manager_update_role_config() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config using hierarchical format
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 2;
    config["capacity"] = 200;
    config["minVoltage"] = 0.2;
    config["maxVoltage"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Update succeeded");

    // Verify update via getRolesAsJson
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject roleConfig = doc[0]["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", roleConfig["fluidType"]);
    TEST_ASSERT_EQUAL(2, roleConfig["inst"]);
    TEST_ASSERT_EQUAL(200, roleConfig["capacity"]);
}

// Tests that applyRoleConfig with unknown roleId creates new role (if roleType provided)
void test_role_manager_update_role_config_unknown_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // Without roleType, should fail for unknown roleId
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "NonExistentRole";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 0.5;
    config["maxVoltage"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    // Now creates a new role since roleId not found but falls through to create
    // However, without roleType it should fail
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing roleType for new role", result.error.c_str());
}

// Tests that applyRoleConfig persists updates to filesystem
void test_role_manager_update_role_config_persists() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // Load role with specific ID (no persist)
    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config using hierarchical format (with persist)
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 2;
    config["capacity"] = 200;
    config["minVoltage"] = 0.2;
    config["maxVoltage"] = 4.8;

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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<512> updateDoc;
    updateDoc["roleId"] = "FluidLevel-abc";
    JsonObject config = updateDoc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 5.0;
    config["maxVoltage"] = 1.0;

    ApplyConfigResult result = manager.applyRoleConfig(updateDoc);
    TEST_ASSERT_FALSE(result.success);
}

// Tests that applyRoleConfig creates a new role and returns generated ID
void test_role_manager_create_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 0.5;
    config["maxVoltage"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());
    TEST_ASSERT_EQUAL(1, manager.roleCount());

    // Verify ID starts with type
    TEST_ASSERT_TRUE(result.roleId.rfind("FluidLevel-", 0) == 0);
}

// Tests that applyRoleConfig fails for unknown type
void test_role_manager_create_role_unknown_type() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 5.0;
    config["maxVoltage"] = 1.0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig persists the new role to filesystem
void test_role_manager_create_role_persists() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 1;
    config["capacity"] = 200;
    config["minVoltage"] = 0.2;
    config["maxVoltage"] = 4.8;

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

// Tests that applyRoleConfig generates unique IDs for multiple roles
void test_role_manager_create_role_unique_ids() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc1;
    doc1["roleType"] = "FluidLevel";
    JsonObject config1 = doc1.createNestedObject("config");
    config1["fluidType"] = "Water";
    config1["inst"] = 0;
    config1["capacity"] = 100;
    config1["minVoltage"] = 0.5;
    config1["maxVoltage"] = 4.5;

    StaticJsonDocument<512> doc2;
    doc2["roleType"] = "FluidLevel";
    JsonObject config2 = doc2.createNestedObject("config");
    config2["fluidType"] = "Fuel";
    config2["inst"] = 1;
    config2["capacity"] = 200;
    config2["minVoltage"] = 0.2;
    config2["maxVoltage"] = 4.8;

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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // Set up files and directory with hierarchical format
    fs.addFile("/roles/FluidLevel-abc.json", R"({
        "roleId": "FluidLevel-abc",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Water",
            "inst": 0,
            "capacity": 100,
            "minVoltage": 0.5,
            "maxVoltage": 4.5
        }
    })");
    fs.addFile("/roles/FluidLevel-xyz.json", R"({
        "roleId": "FluidLevel-xyz",
        "roleType": "FluidLevel",
        "config": {
            "fluidType": "Fuel",
            "inst": 1,
            "capacity": 200,
            "minVoltage": 0.2,
            "maxVoltage": 4.8
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    fs.addDirectory("/roles", {});

    // Should not crash with no roles
    manager.factoryReset();
    manager.loopAll();

    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that factoryReset clears pending persists
void test_role_manager_factory_reset_clears_pending() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // Create a role (adds to pending persist)
    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 0.5;
    config["maxVoltage"] = 4.5;

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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 0.5;
    config["maxVoltage"] = 4.5;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.roleId.empty());
    TEST_ASSERT_TRUE(result.roleId.rfind("FluidLevel-", 0) == 0);
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that applyRoleConfig updates role when roleId provided
void test_role_manager_apply_config_updates_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    // First create a role
    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";
    loadRoleFromJsonString(manager, roleJson, "FluidLevel-xyz");

    // Update via applyRoleConfig
    StaticJsonDocument<512> doc;
    doc["roleId"] = "FluidLevel-xyz";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Fuel";
    config["inst"] = 1;
    config["capacity"] = 200;
    config["minVoltage"] = 0.2;
    config["maxVoltage"] = 4.8;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("FluidLevel-xyz", result.roleId.c_str());

    // Verify update via getRolesAsJson
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> rolesDoc;
    deserializeJson(rolesDoc, json);

    JsonObject roleConfig = rolesDoc[0]["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", roleConfig["fluidType"]);
    TEST_ASSERT_EQUAL(200, roleConfig["capacity"]);
}

// Tests that applyRoleConfig returns correct roleId
void test_role_manager_apply_config_returns_role_id() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleType"] = "FluidLevel";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;
    config["capacity"] = 100;
    config["minVoltage"] = 0.5;
    config["maxVoltage"] = 4.5;

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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    FakeWifiService wifi;
    FakeTcpServer tcp;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea, wifi, tcp);
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
