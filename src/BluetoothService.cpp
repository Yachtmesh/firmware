#include "BluetoothService.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <nvs_flash.h>

#ifndef DEFAULT_BLE_PASSWORD
#define DEFAULT_BLE_PASSWORD "yachtmesh123"
#endif

static const char* TAG = "ble";

BluetoothService::BluetoothService(RoleManager* roleManager,
                                   DeviceInfo* deviceInfo)
    : roleManager_(roleManager), deviceInfo_(deviceInfo) {}

void BluetoothService::start() {
    displayName_ = loadDisplayName();

    if (deviceInfo_) {
        deviceInfo_->start();
    }
    std::string deviceName = "Yachtmesh-";
    deviceName += deviceInfo_ ? deviceInfo_->getDeviceId() : "Unknown";

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(9);  // 9 dBm

    pServer_ = NimBLEDevice::createServer();
    pServer_->setCallbacks(this);

    NimBLEService* pService = pServer_->createService(SERVICE_UUID);

    // Password characteristic - write only
    pPasswordChar_ = pService->createCharacteristic(PASSWORD_CHAR_UUID,
                                                    NIMBLE_PROPERTY::WRITE);
    pPasswordChar_->setCallbacks(this);

    // Auth status characteristic - read + notify
    pAuthStatusChar_ = pService->createCharacteristic(
        AUTH_STATUS_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pAuthStatusChar_->setCallbacks(this);

    // Device info characteristic - read only (requires auth)
    pDeviceInfoChar_ = pService->createCharacteristic(DEVICE_INFO_CHAR_UUID,
                                                      NIMBLE_PROPERTY::READ);
    pDeviceInfoChar_->setCallbacks(this);

    // Status characteristic - read + notify (requires auth)
    pStatusChar_ = pService->createCharacteristic(
        STATUS_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pStatusChar_->setCallbacks(this);

    // Roles characteristic - read + notify (requires auth)
    // Returns JSON: [{id, type, status}, ...]; notified on any role state change
    pRolesChar_ = pService->createCharacteristic(
        ROLES_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pRolesChar_->setCallbacks(this);

    // Config update characteristic - write only (requires auth)
    pConfigUpdateChar_ = pService->createCharacteristic(CONFIG_UPDATE_CHAR_UUID,
                                                        NIMBLE_PROPERTY::WRITE);
    pConfigUpdateChar_->setCallbacks(this);

    // Factory reset characteristic - write only (requires auth)
    pFactoryResetChar_ = pService->createCharacteristic(FACTORY_RESET_CHAR_UUID,
                                                        NIMBLE_PROPERTY::WRITE);
    pFactoryResetChar_->setCallbacks(this);

    // Config request/response: client writes role ID, server notifies with full
    // config
    pConfigRequestChar_ = pService->createCharacteristic(
        CONFIG_REQUEST_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
    pConfigRequestChar_->setCallbacks(this);

    pConfigResponseChar_ = pService->createCharacteristic(
        CONFIG_RESPONSE_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pConfigResponseChar_->setCallbacks(this);

    // Role delete characteristic - write only (requires auth)
    pRoleDeleteChar_ = pService->createCharacteristic(ROLE_DELETE_CHAR_UUID,
                                                      NIMBLE_PROPERTY::WRITE);
    pRoleDeleteChar_->setCallbacks(this);

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(deviceName);
    pAdvertising->setScanResponseData(scanResponse);
    pAdvertising->start();

    ESP_LOGI(TAG, "BLE started as %s", deviceName.c_str());
}

void BluetoothService::stop() {
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
    pServer_ = nullptr;
    authenticatedClients_.clear();
}

void BluetoothService::loop() {
    updateStatus();

    if (!roleManager_) {
        return;
    }

    if (pendingFactoryReset_) {
        pendingFactoryReset_ = false;
        roleManager_->factoryReset();
        displayName_ = "";
        saveDisplayName("");
        ESP_LOGI(TAG, "BLE factory reset initiated");
    }

    if (!pendingConfigUpdate_.empty()) {
        std::string payload = std::move(pendingConfigUpdate_);
        uint16_t connHandle = pendingConfigConnHandle_;
        pendingConfigConnHandle_ = 0;

        StaticJsonDocument<128> ack;
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, payload)) {
            ESP_LOGW(TAG, "BLE config update failed: invalid JSON");
            ack["status"] = "error";
            ack["message"] = "invalid JSON";
        } else if (doc.containsKey("displayName") && !doc.containsKey("roleType")) {
            // Device-level config — display name update
            const char* name = doc["displayName"] | "";
            if (strlen(name) > DISPLAY_NAME_MAX_LEN) {
                ESP_LOGW(TAG, "BLE display name too long (%d chars, max %d)",
                         (int)strlen(name), (int)DISPLAY_NAME_MAX_LEN);
                ack["status"] = "error";
                ack["message"] = "displayName exceeds 64 characters";
            } else {
                displayName_ = name;
                saveDisplayName(displayName_);
                ESP_LOGI(TAG, "BLE display name set: \"%s\"", displayName_.c_str());
                ack["status"] = "ok";
            }
        } else {
            // Role config — forward to role manager
            ApplyConfigResult result = roleManager_->applyRoleConfig(doc);
            if (result.success) {
                ESP_LOGI(TAG, "BLE config applied for role: %s",
                         result.roleId.c_str());
                ack["status"] = "ok";
                ack["id"] = result.roleId;
            } else {
                ESP_LOGW(TAG, "BLE config update failed: %s",
                         result.error.c_str());
                ack["status"] = "error";
                ack["message"] = result.error;
            }
        }

        std::string ackJson;
        serializeJson(ack, ackJson);
        pConfigResponseChar_->setValue(ackJson);
        pConfigResponseChar_->notify(connHandle);
    }
}

std::string BluetoothService::getDeviceId() const {
    return deviceInfo_ ? deviceInfo_->getDeviceId() : "";
}

bool BluetoothService::isConnected() const {
    return pServer_ && pServer_->getConnectedCount() > 0;
}

bool BluetoothService::hasAuthenticatedClient() const {
    return !authenticatedClients_.empty();
}

float BluetoothService::getCpuTemperature() {
    return deviceInfo_ ? deviceInfo_->getCpuTemperature() : 0.0f;
}

void BluetoothService::onConnect(NimBLEServer* pServer,
                                 NimBLEConnInfo& connInfo) {
    ESP_LOGI(TAG, "BLE client connected: %d", connInfo.getConnHandle());
}

void BluetoothService::onDisconnect(NimBLEServer* pServer,
                                    NimBLEConnInfo& connInfo, int reason) {
    uint16_t connHandle = connInfo.getConnHandle();
    authenticatedClients_.erase(connHandle);
    ESP_LOGI(TAG, "BLE client disconnected: %d (reason %d)", connHandle,
             reason);

    NimBLEDevice::startAdvertising();
}

void BluetoothService::onWrite(NimBLECharacteristic* pCharacteristic,
                               NimBLEConnInfo& connInfo) {
    uint16_t connHandle = connInfo.getConnHandle();

    if (pCharacteristic == pPasswordChar_) {
        std::string value = pCharacteristic->getValue();
        uint8_t authResult = 0;

        if (value == DEFAULT_BLE_PASSWORD) {
            authenticatedClients_.insert(connHandle);
            authResult = 1;
            ESP_LOGI(TAG, "BLE client %d authenticated", connHandle);
        } else {
            ESP_LOGW(TAG, "BLE client %d auth failed", connHandle);
        }

        // Notify auth status change
        pAuthStatusChar_->setValue(&authResult, 1);
        pAuthStatusChar_->notify(connHandle);
        return;
    }

    if (!isClientAuthenticated(connHandle)) {
        ESP_LOGW(TAG, "BLE write rejected: not authenticated");
        return;
    }

    if (!roleManager_) {
        ESP_LOGW(TAG, "BLE write rejected: no role manager");
        return;
    }

    if (pCharacteristic == pConfigUpdateChar_) {
        pendingConfigUpdate_ = pCharacteristic->getValue();
        pendingConfigConnHandle_ = connHandle;
        ESP_LOGI(TAG, "BLE config update payload (%d bytes): %.*s",
                 (int)pendingConfigUpdate_.size(),
                 (int)pendingConfigUpdate_.size(),
                 pendingConfigUpdate_.c_str());
    } else if (pCharacteristic == pFactoryResetChar_) {
        pendingFactoryReset_ = true;
        ESP_LOGI(TAG, "BLE factory reset queued");
    } else if (pCharacteristic == pConfigRequestChar_) {
        std::string key = pCharacteristic->getValue();
        if (key == "__device__") {
            StaticJsonDocument<128> doc;
            doc["displayName"] = displayName_;
            std::string json;
            serializeJson(doc, json);
            pConfigResponseChar_->setValue(json);
            pConfigResponseChar_->notify(connHandle);
            ESP_LOGI(TAG, "BLE device config response sent");
        } else {
            std::string json = roleManager_->getRoleConfigJson(key.c_str());
            pConfigResponseChar_->setValue(json);
            pConfigResponseChar_->notify(connHandle);
            ESP_LOGI(TAG, "BLE config response sent for role: %s", key.c_str());
        }
    } else if (pCharacteristic == pRoleDeleteChar_) {
        std::string roleId = pCharacteristic->getValue();
        roleManager_->removeRole(roleId.c_str());
        ESP_LOGI(TAG, "BLE role delete queued: %s", roleId.c_str());
    }
}

void BluetoothService::onRead(NimBLECharacteristic* pCharacteristic,
                              NimBLEConnInfo& connInfo) {
    uint16_t connHandle = connInfo.getConnHandle();
    bool authenticated = isClientAuthenticated(connHandle);

    // Auth status - always readable
    if (pCharacteristic == pAuthStatusChar_) {
        uint8_t val = authenticated ? 1 : 0;
        pCharacteristic->setValue(&val, 1);
        return;
    }

    // Device info, status, and roles require authentication
    if (!authenticated) {
        if (pCharacteristic == pDeviceInfoChar_) {
            uint8_t empty[DeviceInfo::DEVICE_INFO_SIZE] = {0};
            pCharacteristic->setValue(empty, sizeof(empty));
        } else if (pCharacteristic == pStatusChar_) {
            uint8_t empty[DeviceInfo::STATUS_SIZE] = {0};
            pCharacteristic->setValue(empty, sizeof(empty));
        } else if (pCharacteristic == pRolesChar_) {
            pCharacteristic->setValue("[]");
        }
        return;
    }

    // Authenticated - return real data
    if (pCharacteristic == pDeviceInfoChar_) {
        uint8_t buffer[DeviceInfo::DEVICE_INFO_SIZE];
        if (deviceInfo_) {
            deviceInfo_->buildDeviceInfo(buffer);
        } else {
            memset(buffer, 0, sizeof(buffer));
        }
        pCharacteristic->setValue(buffer, sizeof(buffer));
    } else if (pCharacteristic == pStatusChar_) {
        uint8_t buffer[DeviceInfo::STATUS_SIZE];
        if (deviceInfo_) {
            deviceInfo_->buildDeviceStatus(buffer);
        } else {
            memset(buffer, 0, sizeof(buffer));
        }
        pCharacteristic->setValue(buffer, sizeof(buffer));
    } else if (pCharacteristic == pRolesChar_) {
        std::string json = buildRolesJson();
        pCharacteristic->setValue(json);
    }
}

bool BluetoothService::isClientAuthenticated(uint16_t connHandle) const {
    return authenticatedClients_.find(connHandle) !=
           authenticatedClients_.end();
}

void BluetoothService::updateStatus() {
    if (!pServer_ || pServer_->getConnectedCount() == 0) {
        return;
    }

    // Rate limit status and roles updates to avoid flooding the connection
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now - lastStatusUpdate_ < STATUS_UPDATE_INTERVAL_MS) {
        return;
    }
    lastStatusUpdate_ = now;

    // Device status
    if (deviceInfo_) {
        uint8_t buffer[DeviceInfo::STATUS_SIZE];
        deviceInfo_->buildDeviceStatus(buffer);
        pStatusChar_->setValue(buffer, sizeof(buffer));
        pStatusChar_->notify();
    }

    // Roles: notify only when content has changed
    if (roleManager_) {
        std::string rolesJson = buildRolesJson();
        if (rolesJson != lastRolesJson_) {
            lastRolesJson_ = rolesJson;
            pRolesChar_->setValue(rolesJson);
            pRolesChar_->notify();
        }
    }
}

std::string BluetoothService::buildRolesJson() {
    if (!roleManager_) {
        return "[]";
    }
    return roleManager_->getRolesAsJson();
}

std::string BluetoothService::loadDisplayName() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return "";
    }

    size_t required_size = 0;
    esp_err_t err =
        nvs_get_str(handle, NVS_DISPLAY_NAME_KEY, nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        nvs_close(handle);
        return "";
    }

    char* buf = new char[required_size];
    err = nvs_get_str(handle, NVS_DISPLAY_NAME_KEY, buf, &required_size);
    nvs_close(handle);

    std::string name;
    if (err == ESP_OK) {
        name = buf;
    }
    delete[] buf;
    return name;
}

void BluetoothService::saveDisplayName(const std::string& name) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing display name");
        return;
    }
    nvs_set_str(handle, NVS_DISPLAY_NAME_KEY, name.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}
