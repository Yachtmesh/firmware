#include <unity.h>

#include "test_device_info.h"
#include "test_fluid_level_sensor_role.h"
#include "test_role_manager.h"
#include "test_wifi_gateway_role.h"

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

    // getRolesAsJson tests
    RUN_TEST(test_role_manager_get_roles_as_json_empty);
    RUN_TEST(test_role_manager_get_roles_as_json_not_started);
    RUN_TEST(test_role_manager_get_roles_as_json_after_start);
    RUN_TEST(test_role_manager_get_roles_as_json_multiple);
    RUN_TEST(test_role_manager_get_roles_as_json_config_fields);

    // FluidLevelSensorRole config JSON tests
    RUN_TEST(test_fluid_level_sensor_role_get_config_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json_invalid);

    // FluidType serialization tests
    RUN_TEST(test_fluid_type_to_string_all_values);
    RUN_TEST(test_fluid_type_from_string_all_values);
    RUN_TEST(test_fluid_type_from_string_unknown_returns_unavailable);
    RUN_TEST(test_fluid_type_round_trip);

    // RoleManager update config tests
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

    // factoryReset tests
    RUN_TEST(test_role_manager_factory_reset_clears_roles);
    RUN_TEST(test_role_manager_factory_reset_deletes_files);
    RUN_TEST(test_role_manager_factory_reset_empty);
    RUN_TEST(test_role_manager_factory_reset_clears_pending);

    // applyRoleConfig tests
    RUN_TEST(test_role_manager_apply_config_creates_role);
    RUN_TEST(test_role_manager_apply_config_updates_role);
    RUN_TEST(test_role_manager_apply_config_returns_role_id);
    RUN_TEST(test_role_manager_apply_config_missing_config);
    RUN_TEST(test_role_manager_apply_config_missing_role_type);
    RUN_TEST(test_role_manager_apply_config_unknown_role);

    // DeviceInfo tests
    RUN_TEST(test_device_info_generates_id_from_mac);
    RUN_TEST(test_device_info_id_format);
    RUN_TEST(test_device_info_loads_stored_id);
    RUN_TEST(test_device_info_regenerates_invalid_id);
    RUN_TEST(test_device_info_build_device_info_format);
    RUN_TEST(test_device_info_build_device_status_format);
    RUN_TEST(test_device_info_status_sequence_increments);
    RUN_TEST(test_device_info_uptime_calculation);
    RUN_TEST(test_device_info_cpu_temperature);
    RUN_TEST(test_device_info_id_deterministic);

    // WifiGatewayRole tests
    RUN_TEST(test_wifi_gateway_config_from_json);
    RUN_TEST(test_wifi_gateway_config_default_port);
    RUN_TEST(test_wifi_gateway_config_empty_ssid_fails);
    RUN_TEST(test_wifi_gateway_config_missing_ssid_fails);
    RUN_TEST(test_wifi_gateway_config_json_roundtrip);
    RUN_TEST(test_wifi_gateway_type);
    RUN_TEST(test_wifi_gateway_registers_listener_on_start);
    RUN_TEST(test_wifi_gateway_unregisters_listener_on_stop);
    RUN_TEST(test_wifi_gateway_connects_wifi_on_start);
    RUN_TEST(test_wifi_gateway_disconnects_wifi_on_stop);
    RUN_TEST(test_wifi_gateway_starts_tcp_when_wifi_connected);
    RUN_TEST(test_wifi_gateway_stops_tcp_on_stop);
    RUN_TEST(test_wifi_gateway_forwards_data_to_tcp);
    RUN_TEST(test_wifi_gateway_stops_tcp_on_wifi_disconnect);
    RUN_TEST(test_wifi_gateway_restarts_tcp_on_wifi_reconnect);
    RUN_TEST(test_wifi_gateway_receives_local_sensor_data);

    return UNITY_END();
}
