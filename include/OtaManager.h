#pragma once

#include <ArduinoJson.h>

#include <cstdint>
#include <string>

#include "OtaService.h"
#include "Platform.h"
#include "WifiService.h"
#include "types.h"

#define OTA_STATE_LIST(X) \
    X(Idle)                \
    X(ConnectingWifi)      \
    X(Downloading)         \
    X(Finishing)           \
    X(Success)             \
    X(Failed)              \
    X(Unavailable)

#define CURRENT_ENUM_NAME OtaState
GENERATE_ENUM(OtaState, OTA_STATE_LIST)
GENERATE_TO_STRING(OtaState, OTA_STATE_LIST)
GENERATE_FROM_STRING(OtaState, OTA_STATE_LIST)
#undef CURRENT_ENUM_NAME

struct OtaCommandResult {
    bool success;
    std::string message;
};

// Drives an OTA update over Wi-Fi from a BLE-supplied URL. Plain C++, no
// ESP32 dependencies — natively testable. Extracted as its own class (rather
// than embedded in BluetoothService, which has no native test coverage)
// following the same rationale as DeviceInfo.
class OtaManager {
   public:
    OtaManager(OtaServiceInterface& ota, WifiServiceInterface& wifi,
               PlatformInterface& platform);

    // Device-level Wi-Fi credentials used to connect for a download. Set
    // once (e.g. loaded from NVS at BluetoothService::start()) or whenever
    // updated via the Wi-Fi Credentials BLE characteristic.
    void setWifiCredentials(const std::string& ssid,
                            const std::string& password);

    // Handle a parsed OTA Control command: {"action":"start", "url":...,
    // "version":..., "sha256":...} or {"action":"cancel"}.
    OtaCommandResult handleCommand(const JsonDocument& command);

    // Advance the state machine. Call once per main-loop tick.
    void loop();

    // True once, after a state transition or meaningful download progress,
    // until the next call — tells the caller a fresh notify is warranted.
    bool statusChanged();

    // {"state","bytesRead","version","currentVersion","message"}
    std::string buildStatusJson() const;

    OtaState state() const { return state_; }

    // True once the post-success grace window has elapsed and the caller
    // should actually reboot the device.
    bool shouldReboot() const;

    // Reboot into the newly-flashed image. Only meaningful after
    // shouldReboot() returns true.
    void reboot();

    static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
    static constexpr uint32_t DOWNLOAD_STALL_TIMEOUT_MS = 30000;
    static constexpr uint32_t REBOOT_GRACE_MS = 1500;
    static constexpr size_t PROGRESS_NOTIFY_THRESHOLD_BYTES = 8192;

   private:
    OtaServiceInterface& ota_;
    WifiServiceInterface& wifi_;
    PlatformInterface& platform_;

    OtaState state_ = OtaState::Idle;
    bool dirty_ = true;

    std::string wifiSsid_;
    std::string wifiPassword_;

    std::string pendingUrl_;
    std::string targetVersion_;
    std::string lastMessage_;

    uint32_t wifiConnectStartMs_ = 0;
    uint32_t lastProgressMs_ = 0;
    size_t lastBytesRead_ = 0;
    size_t lastNotifiedBytes_ = 0;
    uint32_t successAtMs_ = 0;

    bool isBusy() const;
    void transitionTo(OtaState next);
    void fail(const std::string& message);
};
