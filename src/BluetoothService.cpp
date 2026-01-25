#include "BluetoothService.h"

#include <Arduino.h>
#include <esp_mac.h>
#include <nvs_flash.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

#ifndef DEFAULT_BLE_PASSWORD
#define DEFAULT_BLE_PASSWORD "yachtmesh123"
#endif

// Data structure sizes
static constexpr size_t DEVICE_INFO_SIZE = 20;
static constexpr size_t STATUS_SIZE = 9;

BluetoothService::BluetoothService(RoleManagerInterface* roleManager)
    : roleManager_(roleManager) {
    loadOrGenerateDeviceId();
}

void BluetoothService::loadOrGenerateDeviceId() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);

    if (prefs.isKey(NVS_DEVICE_ID_KEY)) {
        deviceId_ = prefs.getString(NVS_DEVICE_ID_KEY, "").c_str();
    }

    if (deviceId_.empty() || deviceId_.length() != 6) {
        deviceId_ = generateDeviceId();
        prefs.putString(NVS_DEVICE_ID_KEY, deviceId_.c_str());
    }

    prefs.end();
}

std::string BluetoothService::generateDeviceId() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    // Use last 4 bytes (32 bits) - first bytes are manufacturer prefix
    uint32_t id = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    result.reserve(6);

    for (int i = 0; i < 6; i++) {
        result += charset[id % 36];
        id /= 36;
    }

    return result;
}

void BluetoothService::start() {
    startTime_ = millis();
    std::string deviceName = "Yachtmesh-" + deviceId_;

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
    pRolesChar_ = pService->createCharacteristic(ROLES_CHAR_UUID,
                                                  NIMBLE_PROPERTY::READ);
    pRolesChar_->setCallbacks(this);

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

std::string BluetoothService::getDeviceId() const { return deviceId_; }

bool BluetoothService::isConnected() const {
    return pServer_ && pServer_->getConnectedCount() > 0;
}

bool BluetoothService::hasAuthenticatedClient() const {
    return !authenticatedClients_.empty();
}

float BluetoothService::getCpuTemperature() {
    uint8_t raw = temprature_sens_read();
    return (raw - 32) / 1.8f;
}

uint32_t BluetoothService::getUptime() const {
    return (millis() - startTime_) / 1000;
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
            uint8_t empty[DEVICE_INFO_SIZE] = {0};
            pCharacteristic->setValue(empty, sizeof(empty));
        } else if (pCharacteristic == pStatusChar_) {
            uint8_t empty[STATUS_SIZE] = {0};
            pCharacteristic->setValue(empty, sizeof(empty));
        } else if (pCharacteristic == pRolesChar_) {
            uint8_t empty = 0;
            pCharacteristic->setValue(&empty, 1);
        }
        return;
    }

    // Authenticated - return real data
    if (pCharacteristic == pDeviceInfoChar_) {
        uint8_t buffer[DEVICE_INFO_SIZE];
        buildDeviceInfo(buffer);
        pCharacteristic->setValue(buffer, sizeof(buffer));
    } else if (pCharacteristic == pStatusChar_) {
        uint8_t buffer[STATUS_SIZE];
        buildStatus(buffer);
        pCharacteristic->setValue(buffer, sizeof(buffer));
    } else if (pCharacteristic == pRolesChar_) {
        uint8_t buffer[128];  // Max BLE characteristic size
        size_t len = buildRoles(buffer, sizeof(buffer));
        pCharacteristic->setValue(buffer, len);
    }
}

bool BluetoothService::isClientAuthenticated(uint16_t connHandle) const {
    return authenticatedClients_.find(connHandle) !=
           authenticatedClients_.end();
}

void BluetoothService::buildDeviceInfo(uint8_t* buffer) {
    memset(buffer, 0, DEVICE_INFO_SIZE);

    // Device ID (6 bytes, ASCII)
    memcpy(buffer, deviceId_.c_str(), 6);

    // MAC address (6 bytes)
    esp_read_mac(buffer + 6, ESP_MAC_BT);

    // NMEA address (1 byte)
    buffer[12] = DEFAULT_NMEA_ADDRESS;

    // Firmware version (3 bytes)
    buffer[13] = FW_VERSION_MAJOR;
    buffer[14] = FW_VERSION_MINOR;
    buffer[15] = FW_VERSION_PATCH;

    // Reserved (4 bytes) - already zeroed
}

void BluetoothService::buildStatus(uint8_t* buffer) {
    memset(buffer, 0, STATUS_SIZE);

    // Sequence (1 byte)
    buffer[0] = statusSeq_++;

    // Temperature (4 bytes, float32)
    float temp = getCpuTemperature();
    memcpy(buffer + 1, &temp, sizeof(float));

    // Uptime (4 bytes, uint32)
    uint32_t uptime = getUptime();
    memcpy(buffer + 5, &uptime, sizeof(uint32_t));
}

void BluetoothService::updateStatus() {
    if (!pServer_ || pServer_->getConnectedCount() == 0) {
        return;
    }

    // Build and send status update
    uint8_t buffer[STATUS_SIZE];
    buildStatus(buffer);
    pStatusChar_->setValue(buffer, sizeof(buffer));
    pStatusChar_->notify();
}

size_t BluetoothService::buildRoles(uint8_t* buffer, size_t maxSize) {
    if (!roleManager_) {
        buffer[0] = 0;
        return 1;
    }

    auto roles = roleManager_->getRoleInfo();
    size_t offset = 0;

    // Role count (1 byte)
    buffer[offset++] = static_cast<uint8_t>(roles.size());

    // For each role: id_len (1 byte) + id (variable) + running (1 byte)
    for (const auto& role : roles) {
        size_t idLen = strlen(role.id);
        if (idLen > 255) idLen = 255;

        // Check if we have space
        if (offset + 1 + idLen + 1 > maxSize) {
            break;
        }

        buffer[offset++] = static_cast<uint8_t>(idLen);
        memcpy(buffer + offset, role.id, idLen);
        offset += idLen;
        buffer[offset++] = role.running ? 1 : 0;
    }

    return offset;
}
