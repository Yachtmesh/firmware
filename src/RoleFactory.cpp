#include "RoleFactory.h"

#include <cstring>

#include "FluidLevelSensorRole.h"

RoleFactory::RoleFactory(AnalogInputInterface& analog,
                         Nmea2000ServiceInterface& nmea)
    : analog_(analog), nmea_(nmea) {}

std::unique_ptr<Role> RoleFactory::createRole(const char* type,
                                              const JsonDocument& doc) {
    auto role = createRoleInstance(type);
    if (!role) {
        return nullptr;
    }

    // Parse config based on type
    std::unique_ptr<RoleConfig> config;

    if (strcmp(type, "FluidLevel") == 0) {
        float minV = doc["minVoltage"] | 0.0f;
        float maxV = doc["maxVoltage"] | 0.0f;
        unsigned char inst = doc["inst"] | 0;
        const char* ftStr = doc["fluidType"] | "Unavailable";
        uint16_t cap = doc["capacity"] | 0;

        config = std::make_unique<FluidLevelConfig>(FluidTypeFromString(ftStr),
                                                    inst, cap, minV, maxV);
    }

    if (!config) {
        return nullptr;
    }

    role->configure(*config);
    return role;
}

std::unique_ptr<Role> RoleFactory::createRoleInstance(const char* type) {
    if (strcmp(type, "FluidLevel") == 0) {
        return std::make_unique<FluidLevelSensorRole>(analog_, nmea_);
    }

    // Add more role types here
    return nullptr;
}
