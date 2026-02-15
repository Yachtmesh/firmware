#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "AnalogInputService.h"
#include "BluetoothService.h"
#include "DeviceInfo.h"
#include "Esp32Platform.h"
#include "LittleFSAdapter.h"
#include "NMEA2000Service.h"
#include "RoleFactory.h"
#include "RoleManager.h"
#include "WifiService.h"

static const char* TAG = "main";

Nmea2000Service nmea;
AnalogInputService analogInput;
WifiService wifi;
LittleFSAdapter fileSystem;
Esp32Platform platform;

RoleFactory roleFactory(analogInput, nmea, wifi, platform);
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
