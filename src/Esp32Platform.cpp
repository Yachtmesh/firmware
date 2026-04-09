#include "Esp32Platform.h"

#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_freertos_hooks.h>
#include <nvs.h>
#include <nvs_flash.h>

#if CONFIG_IDF_TARGET_ESP32S3
#include <driver/temperature_sensor.h>
#else
#include <rom/ets_sys.h>
#include <soc/sens_reg.h>
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
#if CONFIG_IDF_TARGET_ESP32S3
    static temperature_sensor_handle_t sensor = nullptr;
    if (!sensor) {
        temperature_sensor_config_t cfg =
            TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
        temperature_sensor_install(&cfg, &sensor);
        temperature_sensor_enable(sensor);
    }
    float celsius = 0.0f;
    temperature_sensor_get_celsius(sensor, &celsius);
    return celsius;
#else
    // ESP-IDF 5.x: ROM temprature_sens_read() is a stub on original ESP32.
    // Read sensor registers directly instead.
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 3,
                      SENS_FORCE_XPD_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_CLK_DIV, 6,
                      SENS_TSENS_CLK_DIV_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_DUMP_OUT);
    SET_PERI_REG_MASK(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_POWER_UP_FORCE);
    SET_PERI_REG_MASK(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_POWER_UP);
    ets_delay_us(100);
    SET_PERI_REG_MASK(SENS_SAR_SLAVE_ADDR4_REG, SENS_TSENS_DUMP_OUT);
    ets_delay_us(5);
    int raw = (int)GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR3_REG, SENS_TSENS_OUT,
                                      SENS_TSENS_OUT_S);
    return (raw - 32) / 1.8f;
#endif
}

uint32_t Esp32Platform::getMillis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// --- Heap ---

uint32_t Esp32Platform::getFreeHeap() const { return esp_get_free_heap_size(); }

uint32_t Esp32Platform::getMinFreeHeap() const {
    return esp_get_minimum_free_heap_size();
}

// --- CPU load (approximate via idle hook) ---
//
// The idle hook increments a counter each time the FreeRTOS idle task runs.
// getCpuLoad() reads the counter accumulated since the last call, resets it,
// and computes load as: 100 - (idleCount / IDLE_MAX_PER_SEC * 100).
//
// IDLE_MAX_PER_SEC is the idle call count observed when fully idle. With the
// default ESP-IDF tick rate of 1000 Hz and no other work, the idle task runs
// approximately once per tick, giving ~1000 calls/s. This is an approximation
// — reported load will be slightly off if tick rate differs, but is accurate
// enough for a dashboard indicator.

static constexpr uint32_t IDLE_MAX_PER_SEC = 1000;
static volatile uint32_t idleCounter = 0;

static bool idleHook() {
    idleCounter = idleCounter + 1;
    return true;
}

void Esp32Platform::installIdleHook() {
    esp_register_freertos_idle_hook(idleHook);
}

uint8_t Esp32Platform::getCpuLoad() {
    uint32_t count = idleCounter;
    idleCounter = 0;
    uint32_t idlePct =
        (count >= IDLE_MAX_PER_SEC) ? 100 : (count * 100 / IDLE_MAX_PER_SEC);
    return (uint8_t)(100 - idlePct);
}
