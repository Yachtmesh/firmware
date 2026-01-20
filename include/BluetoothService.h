#pragma once

#include <string>
#include <set>
#include <NimBLEDevice.h>
#include <Preferences.h>

enum class HealthStatus : uint8_t {
    Healthy = 0,
    Warning = 1,
    Error = 2,
    Unknown = 255
};

class BluetoothServiceInterface {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;

    virtual std::string getDeviceId() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool hasAuthenticatedClient() const = 0;

    virtual void setRunningState(bool running) = 0;
    virtual void setHealthStatus(HealthStatus status) = 0;
    virtual float getCpuTemperature() = 0;

    virtual ~BluetoothServiceInterface() = default;
};

class BluetoothService : public BluetoothServiceInterface,
                         public NimBLEServerCallbacks,
                         public NimBLECharacteristicCallbacks {
public:
    BluetoothService();

    void start() override;
    void stop() override;
    void loop() override;

    std::string getDeviceId() const override;
    bool isConnected() const override;
    bool hasAuthenticatedClient() const override;

    void setRunningState(bool running) override;
    void setHealthStatus(HealthStatus status) override;
    float getCpuTemperature() override;

    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;

    // NimBLECharacteristicCallbacks
    void onWrite(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override;
    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override;

private:
    static constexpr const char* SERVICE_UUID = "4e617669-0001-4d65-7368-000000000001";
    static constexpr const char* AUTH_CHAR_UUID = "4e617669-0001-4d65-7368-000000000002";
    static constexpr const char* RUNNING_CHAR_UUID = "4e617669-0001-4d65-7368-000000000003";
    static constexpr const char* HEALTH_CHAR_UUID = "4e617669-0001-4d65-7368-000000000004";
    static constexpr const char* TEMP_CHAR_UUID = "4e617669-0001-4d65-7368-000000000005";

    static constexpr const char* NVS_NAMESPACE = "yachtmesh";
    static constexpr const char* NVS_DEVICE_ID_KEY = "ble_device_id";

    std::string deviceId_;
    bool running_ = false;
    HealthStatus healthStatus_ = HealthStatus::Unknown;

    NimBLEServer* pServer_ = nullptr;
    NimBLECharacteristic* pAuthChar_ = nullptr;
    NimBLECharacteristic* pRunningChar_ = nullptr;
    NimBLECharacteristic* pHealthChar_ = nullptr;
    NimBLECharacteristic* pTempChar_ = nullptr;

    std::set<uint16_t> authenticatedClients_;

    void loadOrGenerateDeviceId();
    std::string generateDeviceId();
    bool isClientAuthenticated(uint16_t connHandle) const;
    void updateCharacteristics();
};
