#include <unity.h>
#include "test_fluid_level_sensor_role.h"
#include "test_role_manager.h"

int main()
{
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

    return UNITY_END();
}
