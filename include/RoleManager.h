#pragma once
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "FileSystem.h"
#include "RoleFactory.h"

// RoleManager doesn't need to know about AnalogInputInterface or
// Nmea2000ServiceInterface. If we add a new role type that needs GPSInterface,
// only RoleFactory changes - RoleManager remains untouched.

struct RoleInfo {
    const char* id;
    const char* type;
    bool running;
};

class RoleManager {
   public:
    RoleManager(RoleFactory& factory, FileSystemInterface& fs);

    // Scan /roles/ directory and load all configs
    void loadFromDirectory(const char* path = "/roles");

    // Lifecycle management
    void startAll();
    void loopAll();
    void stopAll();

    // Load from a single config file path
    bool loadRole(const char* configPath);

    // Load from JSON string (for testing)
    // instanceId is optional; if not provided, uses role type as ID
    bool loadRoleFromJson(const char* json, const char* instanceId = nullptr);

    // Create a new role from JSON, generating a unique ID
    // Returns the generated role ID, or empty string on failure
    std::string createRole(const char* roleType, const JsonDocument& doc);

    size_t roleCount() const { return roles_.size(); }
    std::vector<RoleInfo> getRoleInfo() const;
    std::string getRoleConfigsJson() const;
    bool updateRoleConfig(const char* roleId, const JsonDocument& doc);

    // Factory reset - clears all roles and their config files
    void factoryReset();

   private:
    RoleFactory& factory_;
    FileSystemInterface& fs_;
    std::vector<std::unique_ptr<Role>> roles_;

    // Cached JSON for thread-safe BLE access
    mutable std::string cachedRolesJson_ = "{}";
    mutable bool cacheValid_ = false;

    void rebuildCache() const;

    // Generate a unique instance ID for a role type
    std::string generateInstanceId(const char* type);

    // Deferred persistence - written in loopAll()
    std::set<std::string> pendingPersist_;
    void persistPendingConfigs();

    // Deferred factory reset - executed in loopAll()
    bool pendingFactoryReset_ = false;
    void executeFactoryReset();
};
