#include <unity.h>

#include "test_fluid_level_sensor_role.h"
#include "test_role_manager.h"

int main() {
    UNITY_BEGIN();

    // FluidLevelSensorRole tests
    RUN_TEST(test_fluid_level_sensor_role_basic_flow);

    // RoleManager tests
    RUN_TEST(test_role_manager_loads_valid_config);
    RUN_TEST(test_role_manager_rejects_unknown_type);
    RUN_TEST(test_role_manager_rejects_invalid_json);
    RUN_TEST(test_role_manager_starts_all_roles);
    RUN_TEST(test_role_manager_loops_all_roles);
    RUN_TEST(test_role_manager_loads_from_file);
    RUN_TEST(test_role_manager_loads_from_directory);

    // RoleInfo tests (for BLE characteristic)
    RUN_TEST(test_role_manager_get_role_info_empty);
    RUN_TEST(test_role_manager_get_role_info_not_started);
    RUN_TEST(test_role_manager_get_role_info_after_start);
    RUN_TEST(test_role_manager_get_role_info_multiple_roles);

    // FluidLevelSensorRole config JSON tests
    RUN_TEST(test_fluid_level_sensor_role_get_config_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json_invalid);

    // FluidType serialization tests
    RUN_TEST(test_fluid_type_to_string_all_values);
    RUN_TEST(test_fluid_type_from_string_all_values);
    RUN_TEST(test_fluid_type_from_string_unknown_returns_unavailable);
    RUN_TEST(test_fluid_type_round_trip);

    // RoleManager config JSON tests
    RUN_TEST(test_role_manager_get_configs_json_empty);
    RUN_TEST(test_role_manager_get_configs_json_single_role);
    RUN_TEST(test_role_manager_update_role_config);
    RUN_TEST(test_role_manager_update_role_config_persists);
    RUN_TEST(test_role_manager_update_role_config_unknown_role);
    RUN_TEST(test_role_manager_update_role_config_invalid);

    // createRole tests
    RUN_TEST(test_role_manager_create_role);
    RUN_TEST(test_role_manager_create_role_unknown_type);
    RUN_TEST(test_role_manager_create_role_missing_type);
    RUN_TEST(test_role_manager_create_role_invalid_config);
    RUN_TEST(test_role_manager_create_role_persists);
    RUN_TEST(test_role_manager_create_role_unique_ids);

    return UNITY_END();
}
