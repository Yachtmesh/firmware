#pragma once
#include <ArduinoJson.h>
#include <unity.h>

#include "FileSystem.h"
#include "MockFileSystem.h"
#include "Role.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "test_fluid_level_sensor_role.h"  // For FakeAnalogInput, FakeNmea2000Service

// Test helper: parse JSON string and load role
// Returns true if parsing and loading succeeded
inline bool loadRoleFromJsonString(RoleManager& manager, const char* json,
                                   const char* instanceId = nullptr) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json)) {
        return false;
    }
    const char* type = doc["type"] | "";
    std::string id = instanceId ? instanceId : (std::string(type) + "-test");
    return manager.loadRole(doc, id.c_str());
}

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

    bool loaded = loadRoleFromJsonString(manager, json);
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

    bool loaded = loadRoleFromJsonString(manager, json);
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

    bool loaded = loadRoleFromJsonString(manager, json);
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
    fs.addDirectory("/roles", {"/roles/FluidLevel-atd.json"});

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(1, manager.roleCount());
}

// Tests that loadRolesFromDirectory loads all files
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

    loadRolesFromDirectory(manager, fs, "/roles");
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that getRolesAsJson returns empty array when no roles loaded
void test_role_manager_get_roles_as_json_empty() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    std::string json = manager.getRolesAsJson();
    TEST_ASSERT_EQUAL_STRING("[]", json.c_str());
}

// Tests that getRolesAsJson returns role with running=false before start
void test_role_manager_get_roles_as_json_not_started() {
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

// Tests that updateRole updates existing role
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

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Fuel";
    updateDoc["inst"] = 2;
    updateDoc["capacity"] = 200;
    updateDoc["minVoltage"] = 0.2;
    updateDoc["maxVoltage"] = 4.8;

    bool result = manager.updateRole("FluidLevel-abc", updateDoc);
    TEST_ASSERT_TRUE_MESSAGE(result, "Update succeeded");

    // Verify update via getRolesAsJson
    std::string json = manager.getRolesAsJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);

    JsonObject config = doc[0]["config"];
    TEST_ASSERT_EQUAL_STRING("Fuel", config["fluidType"]);
    TEST_ASSERT_EQUAL(2, config["inst"]);
    TEST_ASSERT_EQUAL(200, config["capacity"]);
}

// Tests that updateRole returns false for unknown role
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

    bool result = manager.updateRole("NonExistentRole", updateDoc);
    TEST_ASSERT_FALSE(result);
}

// Tests that updateRole persists to filesystem
void test_role_manager_update_role_config_persists() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    // Load role with specific ID
    const char* roleJson = R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })";

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Update config
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Fuel";
    updateDoc["inst"] = 2;
    updateDoc["capacity"] = 200;
    updateDoc["minVoltage"] = 0.2;
    updateDoc["maxVoltage"] = 4.8;

    bool result = manager.updateRole("FluidLevel-abc", updateDoc);
    TEST_ASSERT_TRUE(result);

    // Trigger deferred persist via loopAll
    manager.loopAll();

    // Verify file was written
    const std::string* written =
        fs.getWrittenFile("/roles/FluidLevel-abc.json");
    TEST_ASSERT_NOT_NULL(written);

    // Parse and verify content
    StaticJsonDocument<256> savedDoc;
    deserializeJson(savedDoc, *written);
    TEST_ASSERT_EQUAL_STRING("Fuel", savedDoc["fluidType"]);
    TEST_ASSERT_EQUAL(2, savedDoc["inst"]);
    TEST_ASSERT_EQUAL(200, savedDoc["capacity"]);
}

// Tests that updateRole returns false for invalid config
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

    loadRoleFromJsonString(manager, roleJson, "FluidLevel-abc");

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<256> updateDoc;
    updateDoc["fluidType"] = "Water";
    updateDoc["inst"] = 0;
    updateDoc["capacity"] = 100;
    updateDoc["minVoltage"] = 5.0;
    updateDoc["maxVoltage"] = 1.0;

    bool result = manager.updateRole("FluidLevel-abc", updateDoc);
    TEST_ASSERT_FALSE(result);
}

// Tests that createRole creates a new role and returns generated ID
void test_role_manager_create_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;
    doc["capacity"] = 100;
    doc["minVoltage"] = 0.5;
    doc["maxVoltage"] = 4.5;

    std::string roleId = manager.createRole("FluidLevel", doc);
    TEST_ASSERT_FALSE(roleId.empty());
    TEST_ASSERT_EQUAL(1, manager.roleCount());

    // Verify ID starts with type
    TEST_ASSERT_TRUE(roleId.rfind("FluidLevel-", 0) == 0);
}

// Tests that createRole returns empty string for unknown type
void test_role_manager_create_role_unknown_type() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc;

    std::string roleId = manager.createRole("UnknownRole", doc);
    TEST_ASSERT_TRUE(roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that createRole returns empty string for missing/empty type
void test_role_manager_create_role_missing_type() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;

    std::string roleId = manager.createRole("", doc);
    TEST_ASSERT_TRUE(roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that createRole returns empty string for invalid config
void test_role_manager_create_role_invalid_config() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    // Invalid: minVoltage >= maxVoltage
    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;
    doc["capacity"] = 100;
    doc["minVoltage"] = 5.0;
    doc["maxVoltage"] = 1.0;

    std::string roleId = manager.createRole("FluidLevel", doc);
    TEST_ASSERT_TRUE(roleId.empty());
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that createRole persists the new role to filesystem
void test_role_manager_create_role_persists() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Fuel";
    doc["inst"] = 1;
    doc["capacity"] = 200;
    doc["minVoltage"] = 0.2;
    doc["maxVoltage"] = 4.8;

    std::string roleId = manager.createRole("FluidLevel", doc);
    TEST_ASSERT_FALSE(roleId.empty());

    // Trigger deferred persist via loopAll
    manager.loopAll();

    // Verify file was written
    std::string path = "/roles/" + roleId + ".json";
    const std::string* written = fs.getWrittenFile(path);
    TEST_ASSERT_NOT_NULL(written);

    // Parse and verify content
    StaticJsonDocument<256> savedDoc;
    deserializeJson(savedDoc, *written);
    TEST_ASSERT_EQUAL_STRING("FluidLevel", savedDoc["type"]);
    TEST_ASSERT_EQUAL_STRING("Fuel", savedDoc["fluidType"]);
    TEST_ASSERT_EQUAL(1, savedDoc["inst"]);
    TEST_ASSERT_EQUAL(200, savedDoc["capacity"]);
}

// Tests that createRole generates unique IDs for multiple roles
void test_role_manager_create_role_unique_ids() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<256> doc1;
    doc1["fluidType"] = "Water";
    doc1["inst"] = 0;
    doc1["capacity"] = 100;
    doc1["minVoltage"] = 0.5;
    doc1["maxVoltage"] = 4.5;

    StaticJsonDocument<256> doc2;
    doc2["fluidType"] = "Fuel";
    doc2["inst"] = 1;
    doc2["capacity"] = 200;
    doc2["minVoltage"] = 0.2;
    doc2["maxVoltage"] = 4.8;

    std::string id1 = manager.createRole("FluidLevel", doc1);
    std::string id2 = manager.createRole("FluidLevel", doc2);

    TEST_ASSERT_FALSE(id1.empty());
    TEST_ASSERT_FALSE(id2.empty());
    TEST_ASSERT_TRUE(id1 != id2);
    TEST_ASSERT_EQUAL(2, manager.roleCount());
}

// Tests that factoryReset clears all roles
void test_role_manager_factory_reset_clears_roles() {
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    // Set up files and directory
    fs.addFile("/roles/FluidLevel-abc.json", R"({
        "type": "FluidLevel",
        "fluidType": "Water",
        "inst": 0,
        "capacity": 100,
        "minVoltage": 0.5,
        "maxVoltage": 4.5
    })");
    fs.addFile("/roles/FluidLevel-xyz.json", R"({
        "type": "FluidLevel",
        "fluidType": "Fuel",
        "inst": 1,
        "capacity": 200,
        "minVoltage": 0.2,
        "maxVoltage": 4.8
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    // Create a role (adds to pending persist)
    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water";
    doc["inst"] = 0;
    doc["capacity"] = 100;
    doc["minVoltage"] = 0.5;
    doc["maxVoltage"] = 4.5;

    std::string roleId = manager.createRole("FluidLevel", doc);
    TEST_ASSERT_FALSE(roleId.empty());

    // Set up directory for reset
    fs.addDirectory("/roles", {});

    // Factory reset before loopAll would persist
    manager.factoryReset();
    manager.loopAll();

    // Verify role was not persisted (factory reset cleared pending)
    std::string path = "/roles/" + roleId + ".json";
    const std::string* written = fs.getWrittenFile(path);
    TEST_ASSERT_NULL(written);
    TEST_ASSERT_EQUAL(0, manager.roleCount());
}

// Tests that applyRoleConfig creates role when no roleId provided
void test_role_manager_apply_config_creates_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
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

// Tests that applyRoleConfig fails on unknown role for update
void test_role_manager_apply_config_unknown_role() {
    FakeAnalogInput analog;
    FakeNmea2000Service nmea;
    MockFileSystem fs;
    RoleFactory factory(analog, nmea);
    RoleManager manager(factory, fs);

    StaticJsonDocument<512> doc;
    doc["roleId"] = "NonExistent-xyz";
    JsonObject config = doc.createNestedObject("config");
    config["fluidType"] = "Water";
    config["inst"] = 0;

    ApplyConfigResult result = manager.applyRoleConfig(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Failed to update role", result.error.c_str());
}
