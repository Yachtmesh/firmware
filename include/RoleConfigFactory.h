#pragma once
#include <cstring>
#include <memory>

#include "Role.h"

// Factory function that parses JSON and returns appropriate RoleConfig
// Returns nullptr if type is unknown or parsing fails
std::unique_ptr<RoleConfig> createRoleConfig(const JsonDocument& doc);
