#ifdef ESP32
#include "I2cBusService.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char* TAG = "I2cBus";

Esp32I2cBus::Esp32I2cBus(int sdaPin, int sclPin, i2c_port_t port) {
    i2c_master_bus_config_t config = {
        .i2c_port = port,
        .sda_io_num = (gpio_num_t)sdaPin,
        .scl_io_num = (gpio_num_t)sclPin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };
    esp_err_t err = i2c_new_master_bus(&config, &busHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(err));
    }
}

Esp32I2cBus::~Esp32I2cBus() {
    for (auto& [addr, dev] : devices_) {
        i2c_master_bus_rm_device(dev);
    }
    i2c_del_master_bus(busHandle_);
}

i2c_master_dev_handle_t Esp32I2cBus::getOrAddDevice(uint8_t addr) {
    auto it = devices_.find(addr);
    if (it != devices_.end()) {
        return it->second;
    }
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev;
    esp_err_t err = i2c_master_bus_add_device(busHandle_, &dev_config, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device 0x%02X: %s", addr, esp_err_to_name(err));
        return nullptr;
    }
    devices_[addr] = dev;
    return dev;
}

bool Esp32I2cBus::readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    auto dev = getOrAddDevice(addr);
    if (!dev) return false;
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, pdMS_TO_TICKS(10)) == ESP_OK;
}

bool Esp32I2cBus::writeBytes(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t len) {
    if (len > 31) return false;
    auto dev = getOrAddDevice(addr);
    if (!dev) return false;
    uint8_t tx[32];
    tx[0] = reg;
    memcpy(tx + 1, buf, len);
    return i2c_master_transmit(dev, tx, 1 + len, pdMS_TO_TICKS(10)) == ESP_OK;
}
#endif
