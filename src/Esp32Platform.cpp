#include "Esp32Platform.h"

#include <esp_mac.h>
#include <esp_timer.h>
#include <nvs.h>
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

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return "";
    }

    size_t required_size = 0;
    err = nvs_get_str(handle, NVS_DEVICE_ID_KEY, nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        nvs_close(handle);
        return "";
    }

    char* buf = new char[required_size];
    err = nvs_get_str(handle, NVS_DEVICE_ID_KEY, buf, &required_size);
    nvs_close(handle);

    std::string id;
    if (err == ESP_OK) {
        id = buf;
    }
    delete[] buf;
    return id;
}

void Esp32Platform::saveDeviceId(const std::string& id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    }

    nvs_set_str(handle, NVS_DEVICE_ID_KEY, id.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

float Esp32Platform::getCpuTemperature() {
    uint8_t raw = temprature_sens_read();
    return (raw - 32) / 1.8f;
}

uint32_t Esp32Platform::getMillis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
