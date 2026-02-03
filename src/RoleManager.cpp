#include "RoleManager.h"

#include <cstdio>
#include <cstdlib>

#ifndef ESP32
// esp_random() not available in native tests, stubbing it out here.
inline uint32_t esp_random() { return static_cast<uint32_t>(rand()); }
#endif

RoleManager::RoleManager(RoleFactory& factory, FileSystemInterface& fs)
    : factory_(factory), fs_(fs) {}

bool RoleManager::loadRole(const JsonDocument& doc, const char* instanceId) {
    const char* type = doc["type"] | "";
    return !addRoleInternal(type, doc, instanceId, false).empty();
}

std::string RoleManager::createRole(const char* roleType,
                                    const JsonDocument& doc) {
    return addRoleInternal(roleType, doc, nullptr, true);
}

std::string RoleManager::addRoleInternal(const char* type,
                                         const JsonDocument& doc,
                                         const char* instanceId, bool persist) {
    if (!type || strlen(type) == 0) {
        return "";
    }

    auto role = factory_.createRole(type, doc);
    if (!role) {
        return "";
    }

    std::string id = instanceId ? instanceId : generateInstanceId(type);
    role->setInstanceId(id);

    if (!role->validate()) {
        return "";
    }

    roles_.push_back(std::move(role));
    cacheValid_ = false;

    if (persist) {
        pendingPersist_.insert(id);
    }

    return id;
}

std::string RoleManager::generateInstanceId(const char* type) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";

    std::string id = type;
    id += "-";

    for (int i = 0; i < 3; i++) {
        id += charset[esp_random() % 36];
    }
    return id;
}

void RoleManager::startAll() {
    for (auto& role : roles_) {
        role->start();
    }
}

void RoleManager::loopAll() {
    for (auto& role : roles_) {
        role->loop();
    }

    // Execute factory reset first if pending (clears all state)
    if (pendingFactoryReset_) {
        executeFactoryReset();
        return;
    }

    // Rebuild cache in main loop context if invalidated
    if (!cacheValid_) {
        rebuildCache();
    }

    // Persist any pending config changes
    if (!pendingPersist_.empty()) {
        persistPendingConfigs();
    }
}

void RoleManager::stopAll() {
    for (auto& role : roles_) {
        role->stop();
    }
}

std::string RoleManager::getRolesAsJson() const {
    if (!cacheValid_) {
        rebuildCache();
    }
    return cachedRolesJson_;
}

void RoleManager::rebuildCache() const {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& role : roles_) {
        JsonObject roleObj = arr.createNestedObject();
        roleObj["id"] = role->id();
        roleObj["type"] = role->type();
        roleObj["running"] = role->status().running;

        // Add config as nested object
        StaticJsonDocument<256> configDoc;
        role->getConfigJson(configDoc);
        JsonObject configObj = roleObj.createNestedObject("config");
        for (JsonPair kv : configDoc.as<JsonObject>()) {
            configObj[kv.key()] = kv.value();
        }
    }

    cachedRolesJson_.clear();
    serializeJson(doc, cachedRolesJson_);
    cacheValid_ = true;
}

bool RoleManager::updateRole(const char* roleId, const JsonDocument& doc) {
    for (auto& role : roles_) {
        if (strcmp(role->id(), roleId) == 0) {  // Locate matching role
            bool result = role->configureFromJson(doc);
            if (result) {
                cacheValid_ = false;
                pendingPersist_.insert(roleId);  // Defer write to loopAll()
            }
            return result;
        }
    }
    return false;
}

ApplyConfigResult RoleManager::applyRoleConfig(const JsonDocument& doc) {
    ApplyConfigResult result;

    // Extract fields from doc
    const char* roleId = doc["roleId"] | "";
    const char* roleType = doc["roleType"] | "";
    JsonObjectConst configObj = doc["config"];

    if (configObj.isNull()) {
        result.error = "Missing config object";
        return result;
    }

    // Create a new doc for just the config
    StaticJsonDocument<256> configDoc;
    for (JsonPairConst kv : configObj) {
        configDoc[kv.key()] = kv.value();
    }

    if (strlen(roleId) == 0) {
        // No roleId - create a new role
        if (strlen(roleType) == 0) {
            result.error = "Missing roleType for new role";
            return result;
        }

        std::string newId = createRole(roleType, configDoc);
        if (newId.empty()) {
            result.error = "Failed to create role";
            return result;
        }

        result.success = true;
        result.roleId = newId;
    } else {
        // Update existing role
        if (!updateRole(roleId, configDoc)) {
            result.error = "Failed to update role";
            return result;
        }

        result.success = true;
        result.roleId = roleId;
    }

    return result;
}

void RoleManager::persistPendingConfigs() {
    for (const auto& roleId : pendingPersist_) {
        for (auto& role : roles_) {
            if (strcmp(role->id(), roleId.c_str()) == 0) {
                std::string path = "/roles/";
                path += roleId;
                path += ".json";

                StaticJsonDocument<256> configDoc;
                role->getConfigJson(configDoc);

                std::string json;
                serializeJson(configDoc, json);

                auto file = fs_.open(path.c_str(), "w");
                if (file && *file) {
                    file->write(json.c_str(), json.length());
                    file->close();
                }
                break;
            }
        }
    }
    pendingPersist_.clear();
}

// Wrap these into one, if false, set to true, and then in main loop will
// execute.
void RoleManager::factoryReset() { pendingFactoryReset_ = true; }

void RoleManager::executeFactoryReset() {
    pendingFactoryReset_ = false;

    // Stop and remove all roles
    for (auto& role : roles_) {
        role->stop();
    }

    // Delete all config files in /roles/
    auto root = fs_.open("/roles");
    if (root && root->isDirectory()) {
        std::vector<std::string> filesToDelete;

        auto file = root->openNextFile();
        while (file) {
            if (!file->isDirectory()) {
                std::string path = "/roles/";
                path += file->name();
                filesToDelete.push_back(path);
            }
            file = root->openNextFile();
        }

        for (const auto& path : filesToDelete) {
            fs_.remove(path.c_str());
        }
    }

    roles_.clear();
    pendingPersist_.clear();
    cacheValid_ = false;
}

// Free function for bootstrapping
void loadRolesFromDirectory(RoleManager& manager, FileSystemInterface& fs,
                            const char* path) {
    auto root = fs.open(path);
    if (!root || !root->isDirectory()) {
        return;
    }

    auto file = root->openNextFile();
    while (file) {
        if (!file->isDirectory()) {
            // Read file contents
            size_t fileSize = file->size();
            char* buffer = new char[fileSize + 1];
            file->readBytes(buffer, fileSize);
            buffer[fileSize] = '\0';

            // Extract instance ID from filename (e.g., "FluidLevel-abc.json" ->
            // "FluidLevel-abc")
            std::string instanceId;
            const char* filename = file->name();
            const char* dot = strrchr(filename, '.');
            if (dot) {
                instanceId = std::string(filename, dot - filename);
            } else {
                instanceId = filename;
            }

            // Parse and load
            StaticJsonDocument<512> doc;
            if (!deserializeJson(doc, buffer, fileSize)) {
                manager.loadRole(doc, instanceId.c_str());
            }

            delete[] buffer;
        }
        file = root->openNextFile();
    }
}
