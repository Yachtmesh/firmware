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

        std::string manifestUrl = command["manifestUrl"] | "";
        if (manifestUrl.empty()) {
            return {false, "manifestUrl required"};
        }
        if (manifestUrl.rfind("https://", 0) != 0) {
            return {false, "manifestUrl must use https"};
        }
        if (wifiSsid_.empty()) {
            return {false, "wifi credentials not set"};
        }

        pendingManifestUrl_ = manifestUrl;
        targetVersion_.clear();
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
                OtaResolvedTarget resolved = fetchAndResolveTarget();
                if (!resolved.ok) {
                    wifi_.disconnect();
                    fail(resolved.error);
                    break;
                }

                targetVersion_ = resolved.version;
                if (ota_.begin(resolved.url)) {
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

OtaResolvedTarget OtaManager::fetchAndResolveTarget() {
    std::string manifestBody;
    if (!ota_.fetchManifest(pendingManifestUrl_, manifestBody)) {
        return {false, "", "", "manifest fetch failed: " + ota_.lastError()};
    }

    StaticJsonDocument<MANIFEST_JSON_CAPACITY> doc;
    if (deserializeJson(doc, manifestBody) != DeserializationError::Ok) {
        return {false, "", "", "invalid manifest json"};
    }

    std::string board = platform_.getBoardName();
    JsonObjectConst target = doc["targets"][board.c_str()];
    if (target.isNull()) {
        return {false, "", "", "no firmware image for board: " + board};
    }

    std::string file = target["file"] | "";
    if (file.empty()) {
        return {false, "", "", "manifest missing file for board: " + board};
    }

    size_t lastSlash = pendingManifestUrl_.rfind('/');
    if (lastSlash == std::string::npos) {
        return {false, "", "", "invalid manifest url"};
    }
    std::string binUrl = pendingManifestUrl_.substr(0, lastSlash + 1) + file;
    if (binUrl.rfind("https://", 0) != 0) {
        return {false, "", "", "invalid manifest url"};
    }

    std::string version = doc["version"] | "";
    return {true, binUrl, version, ""};
}

bool OtaManager::shouldReboot() const {
    if (state_ != OtaState::Success) return false;
    return (platform_.getMillis() - successAtMs_) >= REBOOT_GRACE_MS;
}

void OtaManager::reboot() { ota_.reboot(); }
