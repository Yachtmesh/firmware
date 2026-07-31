#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "AnalogInputManager.h"
#include "BluetoothService.h"
#include "board_config.h"
#include "DeviceInfo.h"
#include "EnvironmentalSensorService.h"
#include "Esp32Platform.h"
#include "I2cBusService.h"
#include "LittleFSAdapter.h"
#include "NMEA2000Service.h"
#include "OtaManager.h"
#include "OtaService.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "SerialSensorService.h"
#include "WifiService.h"

static const char* TAG = "main";

Nmea2000Service nmea;
WifiService wifi;
LittleFSAdapter fileSystem;
Esp32Platform platform;
OtaService otaService;
OtaManager otaManager(otaService, wifi, platform);

Esp32I2cBus i2cBus(BOARD_I2C_SDA, BOARD_I2C_SCL);
AnalogInputManager analogInputManager(i2cBus);
EnvironmentalSensorService envSensor(i2cBus, 0x76);  // BME280 default address
SerialSensorService serialSensor(UART_NUM_2, BOARD_SERIAL_RX, BOARD_SERIAL_TX);

RoleFactory roleFactory(analogInputManager, nmea, wifi, platform, envSensor, serialSensor);
RoleManager roleManager(roleFactory, fileSystem);
DeviceInfo deviceInfo(platform, nmea);
BluetoothService bluetooth(&roleManager, &deviceInfo, &otaManager);

extern "C" void app_main() {
    // NimBLE's host stack logs an INFO line for every GATT procedure
    // (e.g. "GATT procedure initiated: notify"), which fires on every
    // BLE notify and drowns out useful application logs.
    esp_log_level_set("NimBLE", ESP_LOG_WARN);

    platform.installIdleHook();

    if (!fileSystem.begin()) {
        ESP_LOGE(TAG, "LittleFS mount failed");
    }
    fileSystem.mkdir("/roles");

    // Start services
    nmea.start();
    bluetooth.start();

    // Load roles from filesystem and start all roles
    loadRolesFromDirectory(roleManager, fileSystem, "/roles");
    roleManager.startAll();

    // Reaching this point means boot succeeded (filesystem mounted, NMEA
    // and BLE started, roles loaded) — cancel any pending OTA rollback so
    // the bootloader stops treating this image as unconfirmed.
    esp_err_t markValidErr = esp_ota_mark_app_valid_cancel_rollback();
    if (markValidErr != ESP_OK) {
        ESP_LOGW(TAG, "esp_ota_mark_app_valid_cancel_rollback failed: %s",
                 esp_err_to_name(markValidErr));
    }

    // Main loop
    while (true) {
        nmea.loop();
        roleManager.loopAll();
        bluetooth.loop();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
