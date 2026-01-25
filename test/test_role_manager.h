#pragma once
#include <unity.h>
#include "FileSystem.h"
#include "Role.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "MockFileSystem.h"
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

    fs.addFile("/roles/tank.json", R"({
        "type": "FluidLevel",
        "fluidType": "BlackWater",
        "inst": 2,
        "capacity": 50,
        "minVoltage": 0.1,
        "maxVoltage": 3.3
    })");

    bool loaded = manager.loadRole("/roles/tank.json");
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
