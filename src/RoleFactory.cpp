#include "RoleFactory.h"

#include <cstring>

#include "FluidLevelSensorRole.h"
#include "WifiGatewayRole.h"

RoleFactory::RoleFactory(AnalogInputInterface& analog,
                         Nmea2000ServiceInterface& nmea,
                         WifiServiceInterface& wifi)
    : analog_(analog), nmea_(nmea), wifi_(wifi) {}

std::unique_ptr<Role> RoleFactory::createRole(const char* type,
                                              const JsonDocument& doc) {
    auto role = createRoleInstance(type);
    if (!role) {
        return nullptr;
    }

    role->configureFromJson(doc);
    return role;
}

std::unique_ptr<Role> RoleFactory::createRoleInstance(const char* type) {
    if (strcmp(type, "FluidLevel") == 0) {
        return std::make_unique<FluidLevelSensorRole>(analog_, nmea_);
    }
    if (strcmp(type, "WifiGateway") == 0) {
        return std::make_unique<WifiGatewayRole>(nmea_, wifi_);
    }

    return nullptr;
}
