#include <unity.h>

#include "test_ais_encoder.h"
#include "test_ais_n2k_to_0183_converter.h"
#include "test_ais_simulator_role.h"
#include "test_boat_simulator.h"
#include "test_device_info.h"
#include "test_current_sensor_service.h"
#include "test_environmental_sensor_service.h"
#include "test_fluid_level_sensor_role.h"
#include "test_role_manager.h"
#include "test_weather_station_role.h"
#include "test_tcp_server.h"
#include "test_ve_direct_battery_role.h"
#include "test_wifi_gateway_0183_role.h"
#include "test_wifi_gateway_role.h"

int main() {
    UNITY_BEGIN();

    // TcpServer helper tests
    RUN_TEST(test_tcpSendFrame_full_send_returns_true);
    RUN_TEST(test_tcpSendFrame_empty_frame_returns_true);
    RUN_TEST(test_tcpSendFrame_eagain_returns_true_client_kept);
    RUN_TEST(test_tcpSendFrame_partial_write_keeps_connection);
    RUN_TEST(test_tcpSendFrame_connection_error_returns_false);
    RUN_TEST(test_tcpDrainRecv_no_data_returns_true);
    RUN_TEST(test_tcpDrainRecv_with_pending_data_returns_true);
    RUN_TEST(test_tcpDrainRecv_remote_closed_returns_false);

    // EnvironmentalSensorService tests
    RUN_TEST(test_environmental_sensor_init_writes_reset);
    RUN_TEST(test_environmental_sensor_init_writes_ctrl_hum);
    RUN_TEST(test_environmental_sensor_init_writes_ctrl_meas);
    RUN_TEST(test_environmental_sensor_read_triggers_forced_mode);
    RUN_TEST(test_environmental_sensor_wrong_chip_id_returns_invalid);
    RUN_TEST(test_environmental_sensor_init_only_once);
    RUN_TEST(test_environmental_sensor_reads_temperature);
    RUN_TEST(test_environmental_sensor_result_valid_after_init);

    // CurrentSensorService tests
    RUN_TEST(test_current_sensor_writes_calibration_register);
    RUN_TEST(test_current_sensor_writes_config_register);
    RUN_TEST(test_current_sensor_calibrates_only_once);
    RUN_TEST(test_current_sensor_reads_current);
    RUN_TEST(test_current_sensor_reads_bus_voltage);
    RUN_TEST(test_current_sensor_reads_power);
    RUN_TEST(test_current_sensor_calibration_scales_with_shunt);

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
    RUN_TEST(test_role_manager_get_roles_as_json_includes_ip_address);
    RUN_TEST(test_role_manager_ip_address_updates_after_wifi_connects);
    RUN_TEST(test_role_manager_get_roles_as_json_no_ip_for_other_roles);
    RUN_TEST(test_role_manager_get_roles_as_json_config_fields);

    // FluidLevelSensorRole config JSON tests
    RUN_TEST(test_fluid_level_sensor_role_get_config_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json);
    RUN_TEST(test_fluid_level_sensor_role_configure_from_json_invalid);
    RUN_TEST(test_fluid_level_sensor_role_start_claims_sensor);
    RUN_TEST(test_fluid_level_sensor_role_stop_releases_sensor);
    RUN_TEST(test_fluid_level_sensor_role_start_clears_reason);
    RUN_TEST(test_fluid_level_sensor_role_address_conflict);

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
    RUN_TEST(test_role_manager_new_role_start_deferred_to_loop);
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

    // removeRole tests
    RUN_TEST(test_role_manager_remove_role_unknown_id);
    RUN_TEST(test_role_manager_remove_role_removes_from_list);
    RUN_TEST(test_role_manager_remove_role_deletes_file);
    RUN_TEST(test_role_manager_remove_role_clears_pending_persist);

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
    RUN_TEST(test_device_info_nmea_address_from_service);

    // WeatherStationRole tests
    RUN_TEST(test_weather_station_role_type_string);
    RUN_TEST(test_weather_station_role_validate_accepts_valid_config);
    RUN_TEST(test_weather_station_role_validate_rejects_zero_interval);
    RUN_TEST(test_weather_station_role_validate_rejects_too_fast_interval);
    RUN_TEST(test_weather_station_role_validate_accepts_minimum_interval);
    RUN_TEST(test_weather_station_role_configure_from_json_accepts_valid);
    RUN_TEST(test_weather_station_role_configure_from_json_parses_all_fields);
    RUN_TEST(test_weather_station_role_configure_from_json_rejects_invalid_interval);
    RUN_TEST(test_weather_station_role_json_round_trip);
    RUN_TEST(test_weather_station_role_start_sets_running);
    RUN_TEST(test_weather_station_role_start_with_invalid_config_does_not_set_running);
    RUN_TEST(test_weather_station_role_stop_clears_running);
    RUN_TEST(test_weather_station_role_start_clears_reason);
    RUN_TEST(test_weather_station_role_loop_broadcasts_environmental_data);
    RUN_TEST(test_weather_station_role_loop_passes_instance_number);
    RUN_TEST(test_weather_station_role_loop_does_not_broadcast_before_interval);
    RUN_TEST(test_weather_station_role_loop_broadcasts_after_interval_elapses);
    RUN_TEST(test_weather_station_role_loop_skips_invalid_reading);
    RUN_TEST(test_weather_station_role_loop_does_not_run_when_stopped);

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
    RUN_TEST(test_wifi_gateway_stop_sets_reason);
    RUN_TEST(test_wifi_gateway_start_clears_reason);
    RUN_TEST(test_fake_wifi_service_ip_address_returns_set_value);
    RUN_TEST(test_wifi_gateway_get_status_json_running);
    RUN_TEST(test_wifi_gateway_get_status_json_stopped);
    RUN_TEST(test_wifi_gateway_status_json_includes_ip_when_connected);
    RUN_TEST(test_wifi_gateway_status_json_ip_empty_when_disconnected);
    RUN_TEST(test_wifi_gateway_receives_local_sensor_data);

    // AIS N2K encoder tests (PGN 129039)
    RUN_TEST(test_pgn129039_returns_correct_size);
    RUN_TEST(test_pgn129039_buffer_too_small);
    RUN_TEST(test_pgn129039_message_id);
    RUN_TEST(test_pgn129039_mmsi_encoding);
    RUN_TEST(test_pgn129039_position_encoding);
    RUN_TEST(test_pgn129039_negative_position);
    RUN_TEST(test_pgn129039_sog_encoding);
    RUN_TEST(test_pgn129039_cog_encoding);
    RUN_TEST(test_pgn129039_heading_encoding);
    RUN_TEST(test_pgn129039_seconds_encoding);

    // AIS N2K encoder tests (PGN 129809 - Static Data Part A)
    RUN_TEST(test_pgn129809_returns_correct_size);
    RUN_TEST(test_pgn129809_buffer_too_small);
    RUN_TEST(test_pgn129809_mmsi_encoding);
    RUN_TEST(test_pgn129809_message_id);
    RUN_TEST(test_pgn129809_name_uppercase_padded);
    RUN_TEST(test_pgn129809_null_name);

    // BoatSimulator tests
    RUN_TEST(test_boat_simulator_init_count);
    RUN_TEST(test_boat_simulator_initial_positions_in_northsea);
    RUN_TEST(test_boat_simulator_unique_mmsi);
    RUN_TEST(test_boat_simulator_update_moves_boats);
    RUN_TEST(test_boat_simulator_zero_delta_no_change);
    RUN_TEST(test_boat_simulator_dead_reckoning_west);
    RUN_TEST(test_boat_simulator_bounding_box_reversal);
    RUN_TEST(test_boat_simulator_heading_follows_cog);
    RUN_TEST(test_boat_simulator_custom_bounding_box);

    // AisSimulatorRole tests
    RUN_TEST(test_ais_simulator_type);
    RUN_TEST(test_ais_simulator_config_from_json);
    RUN_TEST(test_ais_simulator_config_defaults);
    RUN_TEST(test_ais_simulator_config_json_roundtrip);
    RUN_TEST(test_ais_simulator_start_sets_running);
    RUN_TEST(test_ais_simulator_stop_clears_running);
    RUN_TEST(test_ais_simulator_start_clears_reason);
    RUN_TEST(test_ais_simulator_sends_n2k_on_interval);
    RUN_TEST(test_ais_simulator_round_robin_boats);
    RUN_TEST(test_ais_simulator_no_send_before_interval);
    RUN_TEST(test_ais_simulator_forwarded_to_listener);
    RUN_TEST(test_ais_simulator_mmsi_in_payload);

    // AIS N2K to 0183 converter tests
    RUN_TEST(test_converter_pgn129039_produces_aivdm);
    RUN_TEST(test_converter_pgn129039_checksum_valid);
    RUN_TEST(test_converter_pgn129039_mmsi);
    RUN_TEST(test_converter_pgn129039_position);
    RUN_TEST(test_converter_pgn129039_sog_cog);
    RUN_TEST(test_converter_pgn129039_short_data_returns_empty);
    RUN_TEST(test_converter_pgn129809_produces_aivdm);
    RUN_TEST(test_converter_pgn129809_checksum_valid);
    RUN_TEST(test_converter_pgn129809_mmsi_and_name);
    RUN_TEST(test_converter_pgn129809_short_data_returns_empty);
    RUN_TEST(test_converter_unsupported_pgn_returns_empty);

    // WifiGateway0183Role tests
    RUN_TEST(test_wifi_gateway_0183_config_from_json);
    RUN_TEST(test_wifi_gateway_0183_config_default_port);
    RUN_TEST(test_wifi_gateway_0183_config_empty_ssid_fails);
    RUN_TEST(test_wifi_gateway_0183_config_json_roundtrip);
    RUN_TEST(test_wifi_gateway_0183_type);
    RUN_TEST(test_wifi_gateway_0183_registers_listener_on_start);
    RUN_TEST(test_wifi_gateway_0183_unregisters_listener_on_stop);
    RUN_TEST(test_wifi_gateway_0183_connects_wifi_on_start);
    RUN_TEST(test_wifi_gateway_0183_starts_tcp_when_wifi_connected);
    RUN_TEST(test_wifi_gateway_0183_stops_tcp_on_wifi_disconnect);
    RUN_TEST(test_wifi_gateway_0183_stop_sets_reason);
    RUN_TEST(test_wifi_gateway_0183_start_clears_reason);
    RUN_TEST(test_wifi_gateway_0183_status_json_includes_ip);
    RUN_TEST(test_wifi_gateway_0183_forwards_ais_as_aivdm);
    RUN_TEST(test_wifi_gateway_0183_ignores_unsupported_pgn);
    RUN_TEST(test_wifi_gateway_0183_forwards_static_data);

    // VeDirectParser tests
    RUN_TEST(test_ve_direct_parser_complete_frame_checksums_ok);
    RUN_TEST(test_ve_direct_parser_voltage);
    RUN_TEST(test_ve_direct_parser_current);
    RUN_TEST(test_ve_direct_parser_soc);
    RUN_TEST(test_ve_direct_parser_ttg);
    RUN_TEST(test_ve_direct_parser_consumed_ah);
    RUN_TEST(test_ve_direct_parser_returns_true_on_checksum_line);
    RUN_TEST(test_ve_direct_parser_bad_checksum_marked_invalid);
    RUN_TEST(test_ve_direct_parser_negative_ttg_becomes_unavailable);
    RUN_TEST(test_ve_direct_parser_reset_clears_state);
    RUN_TEST(test_ve_direct_parser_checksum_only_frame_has_no_data);
    RUN_TEST(test_ve_direct_parser_newline_checksum_byte_accepted);

    // VeDirectBatteryRole tests
    RUN_TEST(test_ve_direct_battery_role_type_string);
    RUN_TEST(test_ve_direct_battery_role_validate_always_true);
    RUN_TEST(test_ve_direct_battery_role_start_sets_running);
    RUN_TEST(test_ve_direct_battery_role_stop_clears_running);
    RUN_TEST(test_ve_direct_battery_role_loop_sends_metric_on_complete_frame);
    RUN_TEST(test_ve_direct_battery_role_loop_correct_voltage);
    RUN_TEST(test_ve_direct_battery_role_loop_correct_current);
    RUN_TEST(test_ve_direct_battery_role_loop_correct_soc);
    RUN_TEST(test_ve_direct_battery_role_loop_correct_ttg);
    RUN_TEST(test_ve_direct_battery_role_loop_correct_instance);
    RUN_TEST(test_ve_direct_battery_role_loop_bad_checksum_not_sent);
    RUN_TEST(test_ve_direct_battery_role_loop_no_data_no_send);
    RUN_TEST(test_ve_direct_battery_role_configure_from_json);
    RUN_TEST(test_ve_direct_battery_role_get_config_json);
    RUN_TEST(test_ve_direct_battery_role_config_json_roundtrip);

    return UNITY_END();
}
