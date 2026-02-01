#include "RoleManager.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

RoleManager::RoleManager(RoleFactory& factory, FileSystemInterface& fs)
    : factory_(factory), fs_(fs) {}

void RoleManager::loadFromDirectory(const char* path) {
    auto root = fs_.open(path);
    if (!root || !root->isDirectory()) {
        return;
    }

    auto file = root->openNextFile();
    while (file) {
        if (!file->isDirectory()) {
            // Build full path: path + "/" + filename
            size_t pathLen = strlen(path);
            const char* fileName = file->name();
            size_t nameLen = strlen(fileName);
            size_t fullLen = pathLen + 1 + nameLen + 1;

            char* fullPath = new char[fullLen];
            strcpy(fullPath, path);
            strcat(fullPath, "/");
            strcat(fullPath, fileName);

            loadRole(fullPath);
            delete[] fullPath;
        }
        file = root->openNextFile();
    }
}

bool RoleManager::loadRole(const char* configPath) {
    auto file = fs_.open(configPath, "r");
    if (!file || !(*file)) {
        return false;
    }

    size_t fileSize = file->size();
    char* buffer = new char[fileSize + 1];
    file->readBytes(buffer, fileSize);
    buffer[fileSize] = '\0';
    file->close();

    // Extract filename stem as instance ID (e.g., "/roles/FluidLevel-abc.json"
    // -> "FluidLevel-abc")
    std::string instanceId;
    const char* lastSlash = strrchr(configPath, '/');
    const char* filename = lastSlash ? lastSlash + 1 : configPath;
    const char* dot = strrchr(filename, '.');
    if (dot) {
        instanceId = std::string(filename, dot - filename);
    } else {
        instanceId = filename;
    }

    bool result = parseAndCreateRole(buffer, fileSize, instanceId.c_str());
    delete[] buffer;
    return result;
}

bool RoleManager::loadRoleFromJson(const char* json, const char* instanceId) {
    return parseAndCreateRole(json, strlen(json), instanceId);
}

std::string RoleManager::createRole(const char* roleType,
                                    const JsonDocument& doc) {
    if (!roleType || strlen(roleType) == 0) {
        return "";
    }

    auto role = factory_.createRole(roleType, doc);
    if (!role) {
        return "";
    }

    std::string instanceId = generateInstanceId(roleType);
    role->setInstanceId(instanceId);

    if (!role->validate()) {
        return "";
    }

    roles_.push_back(std::move(role));
    cacheValid_ = false;
    pendingPersist_.insert(instanceId);

    return instanceId;
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

bool RoleManager::parseAndCreateRole(const char* json, size_t length,
                                     const char* instanceId) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, length)) {
        return false;
    }

    // Get type and create configured role
    const char* type = doc["type"] | "";
    auto role = factory_.createRole(type, doc);
    if (!role) {
        return false;
    }

    // Set the instance ID from filename if provided, otherwise use type
    if (instanceId) {
        role->setInstanceId(instanceId);
    } else {
        role->setInstanceId(role->type());
    }

    if (!role->validate()) {
        return false;
    }

    roles_.push_back(std::move(role));
    cacheValid_ = false;  // Invalidate cache when roles change
    return true;
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

std::vector<RoleInfo> RoleManager::getRoleInfo() const {
    std::vector<RoleInfo> info;
    info.reserve(roles_.size());

    for (const auto& role : roles_) {
        RoleStatus s = role->status();
        info.push_back({role->id(), role->type(), s.running});
    }

    return info;
}

std::string RoleManager::getRoleConfigsJson() const {
    if (!cacheValid_) {
        rebuildCache();
    }

    return cachedRolesJson_;
}

void RoleManager::rebuildCache() const {
    StaticJsonDocument<1024> doc;
    doc.to<JsonObject>();  // Initialize as empty object

    for (const auto& role : roles_) {
        JsonObject roleObj = doc.createNestedObject(role->id());
        StaticJsonDocument<256> configDoc;
        role->getConfigJson(configDoc);

        // Copy config fields to the nested object
        for (JsonPair kv : configDoc.as<JsonObject>()) {
            roleObj[kv.key()] = kv.value();
        }
    }

    cachedRolesJson_.clear();
    serializeJson(doc, cachedRolesJson_);

    cacheValid_ = true;
}

bool RoleManager::updateRoleConfig(const char* roleId,
                                   const JsonDocument& doc) {
    for (auto& role : roles_) {
        if (strcmp(role->id(), roleId) == 0) {
            bool result = role->configureFromJson(doc);
            if (result) {
                cacheValid_ = false;  // Invalidate cache on config change
                pendingPersist_.insert(roleId);  // Defer write to loopAll()
            }
            return result;
        }
    }
    return false;
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
