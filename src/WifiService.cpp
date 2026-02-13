#include "WifiService.h"

#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

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

    initialized_ = true;
}

void WifiService::eventHandler(void* arg, esp_event_base_t eventBase,
                               int32_t eventId, void* eventData) {
    auto* self = static_cast<WifiService*>(arg);

    if (eventBase == WIFI_EVENT) {
        if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
            self->connected_ = false;
            ESP_LOGI(TAG, "Disconnected, attempting reconnect...");
            esp_wifi_connect();
        } else if (eventId == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(eventData);
        self->connected_ = true;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

bool WifiService::connect(const char* ssid, const char* password) {
    initWifi();

    // If already connected, disconnect first
    if (connected_) {
        disconnect();
    }

    wifi_config_t wifiConfig = {};
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), ssid,
            sizeof(wifiConfig.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), password,
            sizeof(wifiConfig.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    return true;
}

void WifiService::disconnect() {
    connected_ = false;
    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_LOGI(TAG, "Disconnected");
}

bool WifiService::isConnected() const { return connected_; }
