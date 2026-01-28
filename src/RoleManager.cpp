#include "RoleManager.h"

#include <ArduinoJson.h>

#include <cstring>

#include "RoleConfigFactory.h"

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

    bool result = parseAndCreateRole(buffer, fileSize);
    delete[] buffer;
    return result;
}

bool RoleManager::loadRoleFromJson(const char* json) {
    return parseAndCreateRole(json, strlen(json));
}

bool RoleManager::parseAndCreateRole(const char* json, size_t length) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, length)) {
        return false;
    }

    // Get type and create role
    const char* type = doc["type"] | "";
    auto role = factory_.createRole(type);
    if (!role) {
        return false;
    }

    // Create config and configure role
    auto config = createRoleConfig(doc);
    if (!config) {
        return false;
    }

    role->configure(*config);

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
        info.push_back({role->id(), s.running});
    }

    return info;
}

std::string RoleManager::getRoleConfigsJson() const {
    // Return cached JSON - safe to call from any thread
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

    serializeJson(doc, cachedRolesJson_);
    cacheValid_ = true;
}

bool RoleManager::updateRoleConfig(const char* roleId, const JsonDocument& doc) {
    for (auto& role : roles_) {
        if (strcmp(role->id(), roleId) == 0) {
            bool result = role->configureFromJson(doc);
            if (result) {
                cacheValid_ = false;  // Invalidate cache on config change
            }
            return result;
        }
    }
    return false;
}
