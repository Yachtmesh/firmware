#include "OtaManager.h"

#include <cstring>

OtaManager::OtaManager(OtaServiceInterface& ota, WifiServiceInterface& wifi,
                       PlatformInterface& platform)
    : ota_(ota), wifi_(wifi), platform_(platform) {}

void OtaManager::setWifiCredentials(const std::string& ssid,
                                    const std::string& password) {
    wifiSsid_ = ssid;
    wifiPassword_ = password;
}

bool OtaManager::isBusy() const {
    return state_ == OtaState::ConnectingWifi ||
           state_ == OtaState::Downloading || state_ == OtaState::Finishing;
}

void OtaManager::transitionTo(OtaState next) {
    state_ = next;
    dirty_ = true;
}

void OtaManager::fail(const std::string& message) {
    lastMessage_ = message;
    transitionTo(OtaState::Failed);
}

OtaCommandResult OtaManager::handleCommand(const JsonDocument& command) {
    std::string action = command["action"] | "";

    if (action == "start") {
        if (isBusy()) {
            return {false, "already in progress"};
        }

        std::string url = command["url"] | "";
        if (url.empty()) {
            return {false, "url required"};
        }
        if (url.rfind("https://", 0) != 0) {
            return {false, "url must use https"};
        }
        if (wifiSsid_.empty()) {
            return {false, "wifi credentials not set"};
        }

        pendingUrl_ = url;
        targetVersion_ = std::string(command["version"] | "");
        lastMessage_.clear();
        lastBytesRead_ = 0;
        lastNotifiedBytes_ = 0;
        wifiConnectStartMs_ = platform_.getMillis();
        lastProgressMs_ = wifiConnectStartMs_;

        wifi_.connect(wifiSsid_.c_str(), wifiPassword_.c_str());
        transitionTo(OtaState::ConnectingWifi);
        return {true, "ok"};
    }

    if (action == "cancel") {
        if (!isBusy()) {
            return {false, "not in progress"};
        }
        ota_.abort();
        wifi_.disconnect();
        transitionTo(OtaState::Idle);
        return {true, "ok"};
    }

    return {false, "unknown action"};
}

void OtaManager::loop() {
    switch (state_) {
        case OtaState::ConnectingWifi: {
            if (wifi_.isConnected()) {
                if (ota_.begin(pendingUrl_)) {
                    lastProgressMs_ = platform_.getMillis();
                    transitionTo(OtaState::Downloading);
                } else {
                    wifi_.disconnect();
                    fail(ota_.lastError());
                }
            } else if (platform_.getMillis() - wifiConnectStartMs_ >
                       WIFI_CONNECT_TIMEOUT_MS) {
                wifi_.disconnect();
                fail("wifi connect timeout");
            }
            break;
        }

        case OtaState::Downloading: {
            size_t bytesBefore = lastBytesRead_;
            OtaPerformResult result = ota_.perform();
            size_t bytesNow = ota_.bytesRead();

            if (bytesNow > bytesBefore) {
                lastBytesRead_ = bytesNow;
                lastProgressMs_ = platform_.getMillis();
                if (bytesNow - lastNotifiedBytes_ >=
                    PROGRESS_NOTIFY_THRESHOLD_BYTES) {
                    lastNotifiedBytes_ = bytesNow;
                    dirty_ = true;
                }
            }

            if (result == OtaPerformResult::Complete) {
                transitionTo(OtaState::Finishing);
            } else if (result == OtaPerformResult::Error) {
                wifi_.disconnect();
                fail(ota_.lastError());
            } else if (platform_.getMillis() - lastProgressMs_ >
                       DOWNLOAD_STALL_TIMEOUT_MS) {
                ota_.abort();
                wifi_.disconnect();
                fail("download timeout");
            }
            break;
        }

        case OtaState::Finishing: {
            if (ota_.finish()) {
                successAtMs_ = platform_.getMillis();
                transitionTo(OtaState::Success);
            } else {
                wifi_.disconnect();
                fail(ota_.lastError());
            }
            break;
        }

        default:
            break;
    }
}

bool OtaManager::statusChanged() {
    bool changed = dirty_;
    dirty_ = false;
    return changed;
}

std::string OtaManager::buildStatusJson() const {
    StaticJsonDocument<256> doc;
    doc["state"] = OtaStateToString(state_);
    doc["bytesRead"] = static_cast<uint32_t>(lastBytesRead_);
    doc["version"] = targetVersion_;
    doc["currentVersion"] = platform_.getFirmwareVersion();
    doc["message"] = lastMessage_;

    std::string result;
    serializeJson(doc, result);
    return result;
}

bool OtaManager::shouldReboot() const {
    if (state_ != OtaState::Success) return false;
    return (platform_.getMillis() - successAtMs_) >= REBOOT_GRACE_MS;
}

void OtaManager::reboot() { ota_.reboot(); }
