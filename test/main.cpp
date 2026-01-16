#include <unity.h>
#include "test_fluid_level_sensor_role.h"

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_fluid_level_sensor_role_basic_flow);

    return UNITY_END();
}
