#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BluetoothService.h"
#include "CurrentSensorManager.h"
#include "DeviceInfo.h"
#include "Esp32Platform.h"
#include "I2cBusService.h"
#include "LittleFSAdapter.h"
#include "NMEA2000Service.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "WifiService.h"

static const char* TAG = "main";

Nmea2000Service nmea;
WifiService wifi;
LittleFSAdapter fileSystem;
Esp32Platform platform;

Esp32I2cBus i2cBus(21, 22);  // SDA=21, SCL=22
CurrentSensorManager currentSensorManager(i2cBus);

RoleFactory roleFactory(currentSensorManager, nmea, wifi, platform);
RoleManager roleManager(roleFactory, fileSystem);
DeviceInfo deviceInfo(platform, nmea);
BluetoothService bluetooth(&roleManager, &deviceInfo);

extern "C" void app_main() {
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

    // Main loop
    while (true) {
        nmea.loop();
        roleManager.loopAll();
        bluetooth.loop();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
