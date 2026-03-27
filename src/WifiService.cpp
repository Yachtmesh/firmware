#include "WifiService.h"

#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>

static const char* TAG = "WifiService";

void WifiService::initWifi() {
    if (initialized_) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiService::eventHandler, this,
        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiService::eventHandler, this,
        nullptr));

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = reconnectTimerCallback;
    timerArgs.arg = this;
    timerArgs.name = "wifi_reconnect";
    ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &reconnectTimer_));

    initialized_ = true;
}

void WifiService::eventHandler(void* arg, esp_event_base_t eventBase,
                               int32_t eventId, void* eventData) {
    auto* self = static_cast<WifiService*>(arg);

    if (eventBase == WIFI_EVENT) {
        if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
            self->connected_ = false;
            ESP_LOGI(TAG, "Disconnected, scheduling reconnect in %ums",
                     RECONNECT_DELAY_MS);
            // Delay reconnect to avoid tight loop under BLE/WiFi coexist
            esp_timer_stop(self->reconnectTimer_);  // no-op if not running
            esp_timer_start_once(self->reconnectTimer_,
                                 RECONNECT_DELAY_MS * 1000ULL);
        } else if (eventId == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(eventData);
        self->connected_ = true;
        snprintf(self->ipAddress_, sizeof(self->ipAddress_), IPSTR,
                 IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", self->ipAddress_);
    }
}

bool WifiService::connect(const char* ssid, const char* password) {
    initWifi();

    // WiFi is a shared resource — multiple roles may call connect/disconnect.
    // Reference counting ensures we only tear down when the last user disconnects.
    refCount_++;

    if (started_) {
        ESP_LOGI(TAG, "WiFi already started (refCount=%d, connected=%d)",
                 refCount_, connected_);
        return true;
    }

    wifi_config_t wifiConfig = {};
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), ssid,
            sizeof(wifiConfig.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), password,
            sizeof(wifiConfig.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
    started_ = true;

    ESP_LOGI(TAG, "Connecting to SSID: %s (refCount=%d)", ssid, refCount_);
    return true;
}

void WifiService::disconnect() {
    if (refCount_ > 0) {
        refCount_--;
    }

    if (refCount_ > 0) {
        ESP_LOGI(TAG, "WiFi still in use (refCount=%d), keeping connection", refCount_);
        return;
    }

    connected_ = false;
    started_ = false;
    ipAddress_[0] = '\0';
    esp_timer_stop(reconnectTimer_);  // cancel any pending reconnect
    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_LOGI(TAG, "Disconnected (last user)");
}

void WifiService::reconnectTimerCallback(void* arg) {
    ESP_LOGI(TAG, "Reconnecting...");
    esp_wifi_connect();
}

bool WifiService::isConnected() const { return connected_; }

std::string WifiService::ipAddress() const { return std::string(ipAddress_); }
