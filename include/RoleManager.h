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

class RoleManager {
   public:
    RoleManager(RoleFactory& factory, FileSystemInterface& fs);

    // Lifecycle management
    void startAll();
    void loopAll();
    void stopAll();

    // Load existing role from config (no persistence, ID provided)
    // Doc must contain "type" field
    bool loadRole(const JsonDocument& doc, const char* instanceId);

    // Create a new role from JSON, generating a unique ID
    // Returns the generated role ID, or empty string on failure
    std::string createRole(const char* roleType, const JsonDocument& doc);

    // Update existing role config
    bool updateRole(const char* roleId, const JsonDocument& doc);

    size_t roleCount() const { return roles_.size(); }

    // Returns JSON array of all roles with id, type, running status, and config
    std::string getRolesAsJson() const;

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

    // Core role addition logic - returns instance ID or empty on failure
    std::string addRoleInternal(const char* type, const JsonDocument& doc,
                                const char* instanceId, bool persist);

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
