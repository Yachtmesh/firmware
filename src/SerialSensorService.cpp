#ifdef ESP32
#include "SerialSensorService.h"

#include <esp_log.h>

static const char* TAG = "SerialSensor";
static constexpr int RX_BUF_SIZE = 1024;

SerialSensorService::SerialSensorService(uart_port_t port) : port_(port) {}

void SerialSensorService::begin(int baudRate, int rxPin, int txPin) {
    uart_config_t config = {
        .baud_rate = baudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(port_, &config);
    uart_set_pin(port_, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(port_, RX_BUF_SIZE * 2, 0, 0, nullptr, 0);
    ESP_LOGI(TAG, "UART%d initialized baud=%d rx=%d tx=%d", port_, baudRate, rxPin, txPin);
}

SerialReading SerialSensorService::readLine() {
    uint8_t byte;
    while (uart_read_bytes(port_, &byte, 1, 0) > 0) {
        if (byte == '\n') {
            // Strip trailing '\r' if present
            if (!lineBuffer_.empty() && lineBuffer_.back() == '\r') {
                lineBuffer_.pop_back();
            }
            SerialReading result{lineBuffer_, true};
            lineBuffer_.clear();
            return result;
        }
        lineBuffer_ += (char)byte;
    }
    return {"", false};
}
#endif
