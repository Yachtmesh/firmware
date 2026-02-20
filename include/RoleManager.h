#pragma once
#include <ArduinoJson.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "FileSystem.h"
#include "RoleFactory.h"

// RoleManager doesn't need to know about AnalogInputInterface or
// Nmea2000ServiceInterface. If we add a new role type that needs GPSInterface,
// only RoleFactory changes - RoleManager remains untouched.

// Result of applyRoleConfig operation
struct ApplyConfigResult {
    bool success = false;
    std::string roleId;  // New ID on create, existing ID on update
    std::string error;
};

class RoleManager {
   public:
    RoleManager(RoleFactory& factory, FileSystemInterface& fs);

    // Lifecycle management
    void startAll();
    void loopAll();
    void stopAll();

    // Unified config application - creates or updates roles
    // If roleId matches existing role, updates it
    // If roleId is empty/missing, creates a new role using roleType
    // persist=true for runtime API calls, false when loading from disk
    // Expected JSON format:
    // {"roleId"?: "FluidLevel-abc", "roleType": "FluidLevel", "config": {...}}
    ApplyConfigResult applyRoleConfig(const JsonDocument& doc, bool persist = true);

    size_t roleCount() const { return roles_.size(); }

    // Returns lightweight JSON array of all roles: [{id, type, running}, ...]
    std::string getRolesAsJson() const;

    // Returns full config JSON for a single role: {roleId, roleType, config:{...}}
    // Returns "{}" if roleId not found
    std::string getRoleConfigJson(const char* roleId) const;

    // Factory reset - clears all roles and their config files
    void factoryReset();

   private:
    RoleFactory& factory_;
    FileSystemInterface& fs_;
    std::vector<std::unique_ptr<Role>> roles_;

    // Cached JSON for thread-safe BLE access
    mutable std::string cachedRolesJson_ = "[]";
    mutable bool cacheValid_ = false;

    void rebuildCache() const;

    // Generate a unique instance ID for a role type
    std::string generateInstanceId(const char* type);

    // Find existing role by ID, returns nullptr if not found
    Role* findRole(const char* roleId);

    // Deferred persistence - written in loopAll()
    std::set<std::string> pendingPersist_;
    void persistPendingConfigs();

    // Deferred factory reset - executed in loopAll()
    bool pendingFactoryReset_ = false;
    void executeFactoryReset();
};

// Free function for bootstrapping - loads all role configs from a directory
void loadRolesFromDirectory(RoleManager& manager, FileSystemInterface& fs,
                            const char* path = "/roles");
