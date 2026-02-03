#include "Esp32Platform.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <nvs_flash.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

void Esp32Platform::getMacAddress(uint8_t* mac) {
    esp_read_mac(mac, ESP_MAC_BT);
}

std::string Esp32Platform::loadDeviceId() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // read-only

    std::string id;
    if (prefs.isKey(NVS_DEVICE_ID_KEY)) {
        id = prefs.getString(NVS_DEVICE_ID_KEY, "").c_str();
    }

    prefs.end();
    return id;
}

void Esp32Platform::saveDeviceId(const std::string& id) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);  // read-write
    prefs.putString(NVS_DEVICE_ID_KEY, id.c_str());
    prefs.end();
}

float Esp32Platform::getCpuTemperature() {
    uint8_t raw = temprature_sens_read();
    return (raw - 32) / 1.8f;
}

uint32_t Esp32Platform::getMillis() {
    return millis();
}
