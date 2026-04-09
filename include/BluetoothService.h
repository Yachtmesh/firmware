#pragma once

#include <NimBLEDevice.h>

#include <set>
#include <string>

#include "DeviceInfo.h"
#include "LogStream.h"
#include "RoleManager.h"

class BluetoothServiceInterface {
   public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;

    virtual std::string getDeviceId() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool hasAuthenticatedClient() const = 0;

    virtual float getCpuTemperature() = 0;

    virtual ~BluetoothServiceInterface() = default;
};

class BluetoothService : public BluetoothServiceInterface,
                         public NimBLEServerCallbacks,
                         public NimBLECharacteristicCallbacks {
   public:
    BluetoothService(RoleManager* roleManager = nullptr,
                     DeviceInfo* deviceInfo = nullptr,
                     LogStream* logStream = nullptr);

    void start() override;
    void stop() override;
    void loop() override;

    std::string getDeviceId() const override;
    bool isConnected() const override;
    bool hasAuthenticatedClient() const override;

    float getCpuTemperature() override;

    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                      int reason) override;

    // NimBLECharacteristicCallbacks
    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override;

   private:
    // UUIDs
    static constexpr const char* SERVICE_UUID =
        "4e617669-0001-4d65-7368-000000000001";
    static constexpr const char* PASSWORD_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000002";
    static constexpr const char* AUTH_STATUS_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000003";
    static constexpr const char* DEVICE_INFO_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000004";
    static constexpr const char* STATUS_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000005";
    static constexpr const char* ROLES_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000006";
    static constexpr const char* CONFIG_UPDATE_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000007";
    static constexpr const char* FACTORY_RESET_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000008";
    static constexpr const char* CONFIG_REQUEST_CHAR_UUID =
        "4e617669-0001-4d65-7368-000000000009";
    static constexpr const char* CONFIG_RESPONSE_CHAR_UUID =
        "4e617669-0001-4d65-7368-00000000000a";
    static constexpr const char* ROLE_DELETE_CHAR_UUID =
        "4e617669-0001-4d65-7368-00000000000b";
    static constexpr const char* LOG_CONTROL_CHAR_UUID =
        "4e617669-0001-4d65-7368-00000000000c";
    static constexpr const char* LOG_DATA_CHAR_UUID =
        "4e617669-0001-4d65-7368-00000000000d";

    // NVS keys
    static constexpr const char* NVS_NAMESPACE = "yachtmesh";
    static constexpr const char* NVS_DEVICE_ID_KEY = "ble_device_id";

    // Dependencies
    RoleManager* roleManager_ = nullptr;
    DeviceInfo* deviceInfo_ = nullptr;
    LogStream* logStream_ = nullptr;

    // BLE objects
    NimBLEServer* pServer_ = nullptr;
    NimBLECharacteristic* pPasswordChar_ = nullptr;
    NimBLECharacteristic* pAuthStatusChar_ = nullptr;
    NimBLECharacteristic* pDeviceInfoChar_ = nullptr;
    NimBLECharacteristic* pStatusChar_ = nullptr;
    NimBLECharacteristic* pRolesChar_ = nullptr;
    NimBLECharacteristic* pConfigUpdateChar_ = nullptr;
    NimBLECharacteristic* pFactoryResetChar_ = nullptr;
    NimBLECharacteristic* pConfigRequestChar_ = nullptr;
    NimBLECharacteristic* pConfigResponseChar_ = nullptr;
    NimBLECharacteristic* pRoleDeleteChar_ = nullptr;
    NimBLECharacteristic* pLogControlChar_ = nullptr;
    NimBLECharacteristic* pLogDataChar_ = nullptr;

    std::set<uint16_t> authenticatedClients_;

    // Pending operations — set in BLE callbacks, executed in loop()
    std::string pendingConfigUpdate_;
    uint16_t pendingConfigConnHandle_ = 0;
    bool pendingFactoryReset_ = false;
    int pendingLogStreaming_ = -1;  // -1 = no change, 0 = stop, 1 = start

    // Status/roles update rate limiting
    uint32_t lastStatusUpdate_ = 0;
    static constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 1000;

    // Roles notify cache — compared each tick to detect changes
    std::string lastRolesJson_;

    // Private methods
    bool isClientAuthenticated(uint16_t connHandle) const;
    void updateStatus();
    std::string buildRolesJson();
};
