#include "RoleConfigFactory.h"

FluidType fluidTypeFromString(const char* str) {
    if (strcmp(str, "Fuel") == 0) return FluidType::Fuel;
    if (strcmp(str, "Water") == 0) return FluidType::Water;
    if (strcmp(str, "GrayWater") == 0) return FluidType::GrayWater;
    if (strcmp(str, "LiveWell") == 0) return FluidType::LiveWell;
    if (strcmp(str, "Oil") == 0) return FluidType::Oil;
    if (strcmp(str, "BlackWater") == 0) return FluidType::BlackWater;
    if (strcmp(str, "FuelGasoline") == 0) return FluidType::FuelGasoline;
    if (strcmp(str, "Error") == 0) return FluidType::Error;
    return FluidType::Unavailable;
}

std::unique_ptr<RoleConfig> createRoleConfig(const JsonDocument& doc) {
    const char* type = doc["type"] | "";

    if (strcmp(type, "FluidLevel") == 0) {
        float minV = doc["minVoltage"] | 0.0f;
        float maxV = doc["maxVoltage"] | 0.0f;
        unsigned char inst = doc["inst"] | 0;
        const char* ftStr = doc["fluidType"] | "Unavailable";
        uint16_t cap = doc["capacity"] | 0;

        return std::make_unique<FluidLevelConfig>(
            fluidTypeFromString(ftStr), inst, cap, minV, maxV);
    }

    // Future: add more role types here
    // if (strcmp(type, "temperature") == 0) { ... }

    return nullptr;
}
