#include "RoleConfigFactory.h"

#include "FluidLevelSensorRole.h"

std::unique_ptr<RoleConfig> createRoleConfig(const char* roleType,
                                             const JsonDocument& doc) {
    if (strcmp(roleType, "FluidLevel") == 0) {
        float minV = doc["minVoltage"] | 0.0f;
        float maxV = doc["maxVoltage"] | 0.0f;
        unsigned char inst = doc["inst"] | 0;
        const char* ftStr = doc["fluidType"] | "Unavailable";
        uint16_t cap = doc["capacity"] | 0;

        return std::make_unique<FluidLevelConfig>(FluidTypeFromString(ftStr),
                                                  inst, cap, minV, maxV);
    }

    // Future: add more role types here
    // if (strcmp(type, "temperature") == 0) { ... }

    return nullptr;
}
