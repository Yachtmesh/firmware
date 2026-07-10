#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include "MockOtaService.h"
#include "MockPlatform.h"
#include "MockWifiService.h"
#include "OtaManager.h"

namespace {

StaticJsonDocument<256> parseCommand(const char* json) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, json);
    return doc;
}

}  // namespace

// --- Command parsing / rejection ---

void test_ota_manager_start_missing_url_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start"})");
    OtaCommandResult result = manager.handleCommand(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL(OtaState::Idle, manager.state());
}

void test_ota_manager_start_non_https_url_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"http://example.com/fw.bin"})");
    OtaCommandResult result = manager.handleCommand(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL(OtaState::Idle, manager.state());
}

void test_ota_manager_start_without_wifi_credentials_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    // No setWifiCredentials() call.

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    OtaCommandResult result = manager.handleCommand(doc);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL(OtaState::Idle, manager.state());
}

void test_ota_manager_unknown_action_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"pause"})");
    OtaCommandResult result = manager.handleCommand(doc);

    TEST_ASSERT_FALSE(result.success);
}

void test_ota_manager_cancel_while_idle_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);

    auto doc = parseCommand(R"({"action":"cancel"})");
    OtaCommandResult result = manager.handleCommand(doc);

    TEST_ASSERT_FALSE(result.success);
}

void test_ota_manager_start_while_busy_rejected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    TEST_ASSERT_EQUAL(OtaState::ConnectingWifi, manager.state());

    OtaCommandResult second = manager.handleCommand(doc);
    TEST_ASSERT_FALSE(second.success);
}

// --- Wi-Fi credential composition ---

void test_ota_manager_start_connects_wifi_with_stored_credentials() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);

    TEST_ASSERT_TRUE(wifi.connectCalled);
    TEST_ASSERT_EQUAL_STRING("BoatWifi", wifi.lastSsid);
    TEST_ASSERT_EQUAL_STRING("secret123", wifi.lastPassword);
    TEST_ASSERT_EQUAL(OtaState::ConnectingWifi, manager.state());
}

// --- State machine happy path ---

void test_ota_manager_connecting_wifi_transitions_to_downloading_once_connected() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    // FakeWifiService reports connected=true immediately after connect().
    manager.loop();

    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());
    TEST_ASSERT_EQUAL_STRING("https://example.com/fw.bin", ota.lastBeginUrl.c_str());
}

void test_ota_manager_downloading_completes_and_finishes_to_success() {
    MockOtaService ota;
    ota.performResults = {OtaPerformResult::InProgress, OtaPerformResult::Complete};
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // ConnectingWifi -> Downloading
    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());

    manager.loop();  // perform() -> InProgress
    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());

    manager.loop();  // perform() -> Complete -> Finishing
    TEST_ASSERT_EQUAL(OtaState::Finishing, manager.state());

    manager.loop();  // finish() -> Success
    TEST_ASSERT_EQUAL(OtaState::Success, manager.state());
    TEST_ASSERT_EQUAL(1, ota.finishCallCount);
}

void test_ota_manager_perform_error_fails() {
    MockOtaService ota;
    ota.performResults = {OtaPerformResult::Error};
    ota.errorMessage = "connection reset";
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    manager.loop();  // perform() -> Error -> Failed

    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());
}

void test_ota_manager_begin_failure_fails() {
    MockOtaService ota;
    ota.beginResult = false;
    ota.errorMessage = "begin failed";
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // ConnectingWifi -> begin() fails -> Failed

    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());
}

void test_ota_manager_finish_failure_fails() {
    MockOtaService ota;
    ota.performResults = {OtaPerformResult::Complete};
    ota.finishResult = false;
    ota.errorMessage = "image validation failed";
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    manager.loop();  // perform() -> Complete -> Finishing
    manager.loop();  // finish() fails -> Failed

    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());
}

// --- Timeouts ---

void test_ota_manager_wifi_connect_timeout_fails() {
    MockOtaService ota;
    FakeWifiService wifi;
    wifi.connected = false;  // Never actually connects
    MockPlatform platform;
    platform.setMillis(0);
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    wifi.connected = false;  // handleCommand's connect() call sets true; force back off
    manager.loop();
    TEST_ASSERT_EQUAL(OtaState::ConnectingWifi, manager.state());

    platform.setMillis(OtaManager::WIFI_CONNECT_TIMEOUT_MS + 1);
    manager.loop();

    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());
}

void test_ota_manager_download_stall_timeout_fails() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    platform.setMillis(0);
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());

    manager.loop();  // perform() InProgress (default), bytesRead stays 0

    platform.setMillis(OtaManager::DOWNLOAD_STALL_TIMEOUT_MS + 1);
    manager.loop();

    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());
    TEST_ASSERT_TRUE(ota.abortCalled);
}

void test_ota_manager_download_progress_resets_stall_timer() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    platform.setMillis(0);
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading

    // Advance time short of the stall timeout, but with progress in between.
    platform.setMillis(OtaManager::DOWNLOAD_STALL_TIMEOUT_MS - 1000);
    ota.bytesReadValue = 1000;
    manager.loop();  // progress observed, resets stall timer

    platform.setMillis(OtaManager::DOWNLOAD_STALL_TIMEOUT_MS + 500);
    manager.loop();  // only ~1500ms since last progress — should not have stalled

    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());
}

// --- Cancel ---

void test_ota_manager_cancel_from_connecting_wifi() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto startDoc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(startDoc);
    TEST_ASSERT_EQUAL(OtaState::ConnectingWifi, manager.state());

    auto cancelDoc = parseCommand(R"({"action":"cancel"})");
    OtaCommandResult result = manager.handleCommand(cancelDoc);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL(OtaState::Idle, manager.state());
    TEST_ASSERT_TRUE(wifi.disconnectCalled);
}

void test_ota_manager_cancel_from_downloading() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto startDoc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(startDoc);
    manager.loop();  // -> Downloading
    TEST_ASSERT_EQUAL(OtaState::Downloading, manager.state());

    auto cancelDoc = parseCommand(R"({"action":"cancel"})");
    manager.handleCommand(cancelDoc);

    TEST_ASSERT_EQUAL(OtaState::Idle, manager.state());
    TEST_ASSERT_TRUE(ota.abortCalled);
    TEST_ASSERT_TRUE(wifi.disconnectCalled);
}

// --- Retry ---

void test_ota_manager_retry_after_failure() {
    MockOtaService ota;
    ota.beginResult = false;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // begin() fails -> Failed
    TEST_ASSERT_EQUAL(OtaState::Failed, manager.state());

    ota.beginResult = true;
    OtaCommandResult retry = manager.handleCommand(doc);
    TEST_ASSERT_TRUE(retry.success);
    TEST_ASSERT_EQUAL(OtaState::ConnectingWifi, manager.state());
}

// --- Status JSON ---

void test_ota_manager_status_json_shape() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    platform.setFirmwareVersion("v1.3.2");
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin","version":"v1.4.0"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    ota.bytesReadValue = 4096;
    manager.loop();

    std::string json = manager.buildStatusJson();
    StaticJsonDocument<256> out;
    TEST_ASSERT_FALSE(deserializeJson(out, json));

    TEST_ASSERT_EQUAL_STRING("Downloading", out["state"] | "");
    TEST_ASSERT_EQUAL_UINT32(4096, out["bytesRead"] | 0);
    TEST_ASSERT_EQUAL_STRING("v1.4.0", out["version"] | "");
    TEST_ASSERT_EQUAL_STRING("v1.3.2", out["currentVersion"] | "");
}

void test_ota_manager_status_json_includes_message_on_failure() {
    MockOtaService ota;
    ota.beginResult = false;
    ota.errorMessage = "network unreachable";
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // begin() fails -> Failed

    std::string json = manager.buildStatusJson();
    StaticJsonDocument<256> out;
    deserializeJson(out, json);

    TEST_ASSERT_EQUAL_STRING("Failed", out["state"] | "");
    TEST_ASSERT_EQUAL_STRING("network unreachable", out["message"] | "");
}

// --- Reboot grace ---

void test_ota_manager_should_not_reboot_immediately_on_success() {
    MockOtaService ota;
    ota.performResults = {OtaPerformResult::Complete};
    FakeWifiService wifi;
    MockPlatform platform;
    platform.setMillis(0);
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    manager.loop();  // -> Finishing
    manager.loop();  // -> Success

    TEST_ASSERT_EQUAL(OtaState::Success, manager.state());
    TEST_ASSERT_FALSE(manager.shouldReboot());
}

void test_ota_manager_should_reboot_after_grace_period() {
    MockOtaService ota;
    ota.performResults = {OtaPerformResult::Complete};
    FakeWifiService wifi;
    MockPlatform platform;
    platform.setMillis(0);
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);
    manager.loop();  // -> Downloading
    manager.loop();  // -> Finishing
    manager.loop();  // -> Success

    platform.setMillis(OtaManager::REBOOT_GRACE_MS + 1);
    TEST_ASSERT_TRUE(manager.shouldReboot());
}

// --- statusChanged() dirty bit ---

void test_ota_manager_status_changed_true_on_transition() {
    MockOtaService ota;
    FakeWifiService wifi;
    MockPlatform platform;
    OtaManager manager(ota, wifi, platform);
    manager.setWifiCredentials("BoatWifi", "secret123");

    manager.statusChanged();  // clear initial dirty bit from construction

    auto doc = parseCommand(R"({"action":"start","url":"https://example.com/fw.bin"})");
    manager.handleCommand(doc);

    TEST_ASSERT_TRUE(manager.statusChanged());
    TEST_ASSERT_FALSE(manager.statusChanged());  // cleared after read
}
