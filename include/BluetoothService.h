#pragma once

#include <string>
#include <set>
#include <NimBLEDevice.h>
#include <Preferences.h>

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
    BluetoothService(RoleManagerInterface* roleManager = nullptr);

    void start() override;
    void stop() override;
    void loop() override;

    std::string getDeviceId() const override;
    bool isConnected() const override;
    bool hasAuthenticatedClient() const override;

    float getCpuTemperature() override;

    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;

    // NimBLECharacteristicCallbacks
    void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override;
    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override;

private:
    // UUIDs
    static constexpr const char* SERVICE_UUID = "4e617669-0001-4d65-7368-000000000001";
    static constexpr const char* PASSWORD_CHAR_UUID = "4e617669-0001-4d65-7368-000000000002";
    static constexpr const char* AUTH_STATUS_CHAR_UUID = "4e617669-0001-4d65-7368-000000000003";
    static constexpr const char* DEVICE_INFO_CHAR_UUID = "4e617669-0001-4d65-7368-000000000004";
    static constexpr const char* STATUS_CHAR_UUID = "4e617669-0001-4d65-7368-000000000005";
    static constexpr const char* ROLES_CHAR_UUID = "4e617669-0001-4d65-7368-000000000006";
    static constexpr const char* CONFIG_UPDATE_CHAR_UUID = "4e617669-0001-4d65-7368-000000000007";

    // NVS keys
    static constexpr const char* NVS_NAMESPACE = "yachtmesh";
    static constexpr const char* NVS_DEVICE_ID_KEY = "ble_device_id";

    // Firmware version
    static constexpr uint8_t FW_VERSION_MAJOR = 0;
    static constexpr uint8_t FW_VERSION_MINOR = 1;
    static constexpr uint8_t FW_VERSION_PATCH = 0;

    // Default NMEA address (will be configurable later)
    static constexpr uint8_t DEFAULT_NMEA_ADDRESS = 22;

    // Dependencies
    RoleManagerInterface* roleManager_ = nullptr;

    // State
    std::string deviceId_;
    uint8_t statusSeq_ = 0;
    uint32_t startTime_ = 0;

    // BLE objects
    NimBLEServer* pServer_ = nullptr;
    NimBLECharacteristic* pPasswordChar_ = nullptr;
    NimBLECharacteristic* pAuthStatusChar_ = nullptr;
    NimBLECharacteristic* pDeviceInfoChar_ = nullptr;
    NimBLECharacteristic* pStatusChar_ = nullptr;
    NimBLECharacteristic* pRolesChar_ = nullptr;
    NimBLECharacteristic* pConfigUpdateChar_ = nullptr;

    std::set<uint16_t> authenticatedClients_;

    // Private methods
    void loadOrGenerateDeviceId();
    std::string generateDeviceId();
    bool isClientAuthenticated(uint16_t connHandle) const;
    void updateStatus();
    void buildDeviceInfo(uint8_t* buffer);
    void buildStatus(uint8_t* buffer);
    std::string buildRolesJson();
    uint32_t getUptime() const;
};
