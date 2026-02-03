#include "BluetoothService.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef DEFAULT_BLE_PASSWORD
#define DEFAULT_BLE_PASSWORD "yachtmesh123"
#endif

BluetoothService::BluetoothService(RoleManager* roleManager,
                                   DeviceInfo* deviceInfo)
    : roleManager_(roleManager), deviceInfo_(deviceInfo) {}

void BluetoothService::start() {
    if (deviceInfo_) {
        deviceInfo_->start();
    }
    std::string deviceName = "Yachtmesh-";
    deviceName += deviceInfo_ ? deviceInfo_->getDeviceId() : "Unknown";

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

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

    // Roles characteristic - read only (requires auth)
    // Returns JSON with role info and configs merged
    pRolesChar_ =
        pService->createCharacteristic(ROLES_CHAR_UUID, NIMBLE_PROPERTY::READ);
    pRolesChar_->setCallbacks(this);

    // Config update characteristic - write only (requires auth)
    pConfigUpdateChar_ = pService->createCharacteristic(CONFIG_UPDATE_CHAR_UUID,
                                                        NIMBLE_PROPERTY::WRITE);
    pConfigUpdateChar_->setCallbacks(this);

    // Factory reset characteristic - write only (requires auth)
    pFactoryResetChar_ = pService->createCharacteristic(FACTORY_RESET_CHAR_UUID,
                                                        NIMBLE_PROPERTY::WRITE);
    pFactoryResetChar_->setCallbacks(this);

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.printf("BLE started as %s\n", deviceName.c_str());
}

void BluetoothService::stop() {
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
    pServer_ = nullptr;
    authenticatedClients_.clear();
}

void BluetoothService::loop() { updateStatus(); }

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
                                 ble_gap_conn_desc* desc) {
    Serial.printf("BLE client connected: %d\n", desc->conn_handle);
}

void BluetoothService::onDisconnect(NimBLEServer* pServer,
                                    ble_gap_conn_desc* desc) {
    uint16_t connHandle = desc->conn_handle;
    authenticatedClients_.erase(connHandle);
    Serial.printf("BLE client disconnected: %d\n", connHandle);

    NimBLEDevice::startAdvertising();
}

void BluetoothService::onWrite(NimBLECharacteristic* pCharacteristic,
                               ble_gap_conn_desc* desc) {
    if (pCharacteristic == pPasswordChar_) {
        std::string value = pCharacteristic->getValue();
        uint8_t authResult = 0;

        if (value == DEFAULT_BLE_PASSWORD) {
            authenticatedClients_.insert(desc->conn_handle);
            authResult = 1;
            Serial.printf("BLE client %d authenticated\n", desc->conn_handle);
        } else {
            Serial.printf("BLE client %d auth failed\n", desc->conn_handle);
        }

        // Notify auth status change
        pAuthStatusChar_->setValue(&authResult, 1);
        pAuthStatusChar_->notify(desc->conn_handle);
    } else if (pCharacteristic == pConfigUpdateChar_) {
        // Require authentication for config updates
        if (!isClientAuthenticated(desc->conn_handle)) {
            Serial.println("BLE config update rejected: not authenticated");
            return;
        }

        if (!roleManager_) {
            Serial.println("BLE config update rejected: no role manager");
            return;
        }

        std::string value = pCharacteristic->getValue();

        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, value)) {
            Serial.println("BLE config update failed: invalid JSON");
            return;
        }

        // Use unified applyRoleConfig method
        ApplyConfigResult result = roleManager_->applyRoleConfig(doc);

        if (result.success) {
            Serial.printf("BLE config applied for role: %s\n",
                          result.roleId.c_str());
        } else {
            Serial.printf("BLE config update failed: %s\n",
                          result.error.c_str());
        }
    } else if (pCharacteristic == pFactoryResetChar_) {
        // Require authentication for factory reset
        if (!isClientAuthenticated(desc->conn_handle)) {
            Serial.println("BLE factory reset rejected: not authenticated");
            return;
        }

        if (!roleManager_) {
            Serial.println("BLE factory reset rejected: no role manager");
            return;
        }

        roleManager_->factoryReset();
        Serial.println("BLE factory reset initiated");
    }
}

void BluetoothService::onRead(NimBLECharacteristic* pCharacteristic,
                              ble_gap_conn_desc* desc) {
    uint16_t connHandle = desc->conn_handle;
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

    if (!deviceInfo_) {
        return;
    }

    // Build and send status update
    uint8_t buffer[DeviceInfo::STATUS_SIZE];
    deviceInfo_->buildDeviceStatus(buffer);
    pStatusChar_->setValue(buffer, sizeof(buffer));
    pStatusChar_->notify();
}

std::string BluetoothService::buildRolesJson() {
    if (!roleManager_) {
        return "[]";
    }
    return roleManager_->getRolesAsJson();
}
