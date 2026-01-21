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

BluetoothService::BluetoothService() { loadOrGenerateDeviceId(); }

void BluetoothService::loadOrGenerateDeviceId() {
    // Initialize NVS
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
    std::string deviceName = "Yachtmesh-" + deviceId_;

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer_ = NimBLEDevice::createServer();
    pServer_->setCallbacks(this);

    NimBLEService* pService = pServer_->createService(SERVICE_UUID);

    // Auth characteristic - write to authenticate, read/notify for result
    pAuthChar_ = pService->createCharacteristic(
        AUTH_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ |
                            NIMBLE_PROPERTY::NOTIFY);
    pAuthChar_->setCallbacks(this);

    // Running characteristic - read + notify
    pRunningChar_ = pService->createCharacteristic(
        RUNNING_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pRunningChar_->setCallbacks(this);

    // Health characteristic - read + notify
    pHealthChar_ = pService->createCharacteristic(
        HEALTH_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pHealthChar_->setCallbacks(this);

    // Temperature characteristic - read + notify
    pTempChar_ = pService->createCharacteristic(
        TEMP_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pTempChar_->setCallbacks(this);

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

void BluetoothService::loop() { updateCharacteristics(); }

std::string BluetoothService::getDeviceId() const { return deviceId_; }

bool BluetoothService::isConnected() const {
    return pServer_ && pServer_->getConnectedCount() > 0;
}

bool BluetoothService::hasAuthenticatedClient() const {
    return !authenticatedClients_.empty();
}

void BluetoothService::setRunningState(bool running) { running_ = running; }

void BluetoothService::setHealthStatus(HealthStatus status) {
    healthStatus_ = status;
}

float BluetoothService::getCpuTemperature() {
    uint8_t raw = temprature_sens_read();
    return (raw - 32) / 1.8f;
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

    // Restart advertising
    NimBLEDevice::startAdvertising();
}

void BluetoothService::onWrite(NimBLECharacteristic* pCharacteristic,
                               ble_gap_conn_desc* desc) {
    if (pCharacteristic == pAuthChar_) {
        std::string value = pCharacteristic->getValue();
        uint8_t authResult = 0;
        if (value == DEFAULT_BLE_PASSWORD) {
            authenticatedClients_.insert(desc->conn_handle);
            authResult = 1;
            Serial.printf("BLE client %d authenticated\n", desc->conn_handle);
        } else {
            Serial.printf("BLE client %d auth failed\n", desc->conn_handle);
        }
        pAuthChar_->setValue(&authResult, 1);
        pAuthChar_->notify(desc->conn_handle);
    }
}

void BluetoothService::onRead(NimBLECharacteristic* pCharacteristic,
                              ble_gap_conn_desc* desc) {
    uint16_t connHandle = desc->conn_handle;

    // Auth characteristic returns current auth status for this client
    if (pCharacteristic == pAuthChar_) {
        uint8_t val = isClientAuthenticated(connHandle) ? 1 : 0;
        pCharacteristic->setValue(&val, 1);
        return;
    }

    if (!isClientAuthenticated(connHandle)) {
        // Return zero/empty data for unauthenticated clients
        if (pCharacteristic == pRunningChar_) {
            uint8_t val = 0;
            pCharacteristic->setValue(&val, 1);
        } else if (pCharacteristic == pHealthChar_) {
            uint8_t val = static_cast<uint8_t>(HealthStatus::Unknown);
            pCharacteristic->setValue(&val, 1);
        } else if (pCharacteristic == pTempChar_) {
            float val = 0.0f;
            pCharacteristic->setValue(val);
        }
        return;
    }

    // Set actual values for authenticated clients
    if (pCharacteristic == pRunningChar_) {
        uint8_t val = running_ ? 1 : 0;
        pCharacteristic->setValue(&val, 1);
    } else if (pCharacteristic == pHealthChar_) {
        uint8_t val = static_cast<uint8_t>(healthStatus_);
        pCharacteristic->setValue(&val, 1);
    } else if (pCharacteristic == pTempChar_) {
        float temp = getCpuTemperature();
        pCharacteristic->setValue(temp);
    }
}

bool BluetoothService::isClientAuthenticated(uint16_t connHandle) const {
    return authenticatedClients_.find(connHandle) !=
           authenticatedClients_.end();
}

void BluetoothService::updateCharacteristics() {
    if (!pServer_ || pServer_->getConnectedCount() == 0) {
        return;
    }

    // Update values for notify (only sent to subscribed clients)
    uint8_t runningVal = running_ ? 1 : 0;
    pRunningChar_->setValue(&runningVal, 1);
    pRunningChar_->notify();

    uint8_t healthVal = static_cast<uint8_t>(healthStatus_);
    pHealthChar_->setValue(&healthVal, 1);
    pHealthChar_->notify();

    float temp = getCpuTemperature();
    pTempChar_->setValue(temp);
    pTempChar_->notify();
}
