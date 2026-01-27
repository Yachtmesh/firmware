#pragma once
#include <memory>
#include <cstring>
#include <ArduinoJson.h>
#include "all.h"
#include "FluidLevelSensorRole.h"

FluidType fluidTypeFromString(const char* str);
const char* fluidTypeToString(FluidType ft);

// Factory function that parses JSON and returns appropriate RoleConfig
// Returns nullptr if type is unknown or parsing fails
std::unique_ptr<RoleConfig> createRoleConfig(const JsonDocument& doc);
