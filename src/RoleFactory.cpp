#include "RoleFactory.h"

#include <cstring>

#include "FluidLevelSensorRole.h"

RoleFactory::RoleFactory(AnalogInputInterface& analog,
                         Nmea2000ServiceInterface& nmea)
    : analog_(analog), nmea_(nmea) {}

std::unique_ptr<Role> RoleFactory::createRole(const char* type) {
    if (strcmp(type, "FluidLevel") == 0) {
        return std::make_unique<FluidLevelSensorRole>(analog_, nmea_);
    }

    // Add more role types here
    return nullptr;
}
