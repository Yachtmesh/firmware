#pragma once
#include <memory>
#include <vector>

#include "FileSystem.h"
#include "Role.h"
#include "RoleFactory.h"

// RoleManager doesn't need to know about AnalogInputInterface or
// Nmea2000ServiceInterface. If we add a new role type that needs GPSInterface,
// only RoleFactory changes - RoleManager remains untouched.

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
    bool loadRoleFromJson(const char* json);

    size_t roleCount() const { return roles_.size(); }

   private:
    RoleFactory& factory_;
    FileSystemInterface& fs_;
    std::vector<std::unique_ptr<Role>> roles_;

    // Internal: parse JSON and create role
    bool parseAndCreateRole(const char* json, size_t length);
};
