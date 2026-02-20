#include "RoleManager.h"

#include <cstdio>
#include <cstdlib>

#ifdef ESP32
#include <esp_random.h>
#else
// esp_random() not available in native tests, stubbing it out here.
inline uint32_t esp_random() { return static_cast<uint32_t>(rand()); }
#endif

RoleManager::RoleManager(RoleFactory& factory, FileSystemInterface& fs)
    : factory_(factory), fs_(fs) {}

Role* RoleManager::findRole(const char* roleId) {
    for (auto& role : roles_) {
        if (strcmp(role->id(), roleId) == 0) {
            return role.get();
        }
    }
    return nullptr;
}

ApplyConfigResult RoleManager::applyRoleConfig(const JsonDocument& doc,
                                               bool persist) {
    ApplyConfigResult result;

    const char* roleId = doc["roleId"] | "";
    const char* roleType = doc["roleType"] | "";
    JsonObjectConst configObj = doc["config"];

    if (configObj.isNull()) {
        result.error = "Missing config object";
        return result;
    }

    // Create a doc for just the config
    StaticJsonDocument<256> configDoc;
    for (JsonPairConst kv : configObj) {
        configDoc[kv.key()] = kv.value();
    }

    // Check if this is an update to an existing role
    if (strlen(roleId) > 0) {
        Role* existing = findRole(roleId);
        if (existing) {
            // Update existing role
            if (!existing->configureFromJson(configDoc)) {
                result.error = "Failed to update role config";
                return result;
            }
            cacheValid_ = false;
            if (persist) {
                pendingPersist_.insert(roleId);
            }
            result.success = true;
            result.roleId = roleId;
            return result;
        }
        // roleId provided but not found - fall through to create with that ID
    }

    // Create new role
    if (strlen(roleType) == 0) {
        result.error = "Missing roleType for new role";
        return result;
    }

    auto role = factory_.createRole(roleType, configDoc);
    if (!role) {
        result.error = "Failed to create role";
        return result;
    }

    std::string id =
        (strlen(roleId) > 0) ? roleId : generateInstanceId(roleType);
    role->setInstanceId(id);

    if (!role->validate()) {
        result.error = "Role validation failed";
        return result;
    }

    Role* rolePtr = role.get();
    roles_.push_back(std::move(role));
    cacheValid_ = false;

    if (persist) {
        pendingPersist_.insert(id);
        rolePtr->start();  // Start immediately for runtime API calls
    }

    result.success = true;
    result.roleId = id;
    return result;
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

    if (!pendingRemove_.empty()) {
        executePendingRemovals();
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
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& role : roles_) {
        JsonObject roleObj = arr.createNestedObject();
        roleObj["id"] = role->id();
        roleObj["type"] = role->type();
        roleObj["running"] = role->status().running;
    }

    cachedRolesJson_.clear();
    serializeJson(doc, cachedRolesJson_);
    cacheValid_ = true;
}

std::string RoleManager::getRoleConfigJson(const char* roleId) const {
    for (const auto& role : roles_) {
        if (strcmp(role->id(), roleId) == 0) {
            StaticJsonDocument<512> doc;
            doc["id"] = role->id();
            doc["type"] = role->type();

            StaticJsonDocument<256> configDoc;
            role->getConfigJson(configDoc);
            doc["config"] = configDoc;

            std::string json;
            serializeJson(doc, json);
            return json;
        }
    }
    return "{}";
}

void RoleManager::persistPendingConfigs() {
    for (const auto& roleId : pendingPersist_) {
        for (auto& role : roles_) {
            if (strcmp(role->id(), roleId.c_str()) == 0) {
                std::string path = "/roles/";
                path += roleId;
                path += ".json";

                // Build hierarchical format: {roleId, roleType, config}
                StaticJsonDocument<512> doc;
                doc["roleId"] = role->id();
                doc["roleType"] = role->type();

                StaticJsonDocument<256> configDoc;
                role->getConfigJson(configDoc);
                doc["config"] = configDoc;

                std::string json;
                serializeJson(doc, json);

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

void RoleManager::removeRole(const char* id) {
    pendingRemove_.insert(id);
}

// Wrap these into one, if false, set to true, and then in main loop will
// execute.
void RoleManager::factoryReset() { pendingFactoryReset_ = true; }

void RoleManager::executePendingRemovals() {
    for (const auto& roleId : pendingRemove_) {
        for (auto it = roles_.begin(); it != roles_.end(); ++it) {
            if (strcmp((*it)->id(), roleId.c_str()) == 0) {
                (*it)->stop();

                std::string path = "/roles/";
                path += roleId;
                path += ".json";
                fs_.remove(path.c_str());

                pendingPersist_.erase(roleId);
                roles_.erase(it);
                break;
            }
        }
        // Unknown ID: silently skip
    }
    pendingRemove_.clear();
    cacheValid_ = false;
}

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

    static constexpr size_t kMaxConfigSize = 2048;

    auto file = root->openNextFile();
    while (file) {
        if (!file->isDirectory()) {
            const char* fileName = file->name();
            size_t fileSize = file->size();

            // Skip if file is huge, config json files are typically below 2kb
            if (fileSize == 0 || fileSize > kMaxConfigSize) {
                file = root->openNextFile();
                continue;
            }

            // Read file contents
            char* buffer = new char[fileSize + 1];
            file->readBytes(buffer, fileSize);
            buffer[fileSize] = '\0';

            // Parse and load - file contains hierarchical format
            // {roleId, roleType, config}
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, buffer, fileSize);
            if (err) {
                printf("loadRoles: JSON error in %s: %s\n", fileName,
                       err.c_str());
            } else {
                manager.applyRoleConfig(doc,
                                        false);  // No persist - already on disk
            }

            delete[] buffer;
        }
        file = root->openNextFile();
    }
}
