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
}

void RoleManager::stopAll() {
    for (auto& role : roles_) {
        role->stop();
    }
}
