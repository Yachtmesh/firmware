#include <Arduino.h>

#include "AnalogInputService.h"
#include "BluetoothService.h"
#include "DeviceInfo.h"
#include "Esp32Platform.h"
#include "LittleFSAdapter.h"
#include "NMEA2000Service.h"
#include "RoleFactory.h"
#include "RoleManager.h"

Nmea2000Service nmea;
AnalogInputService analogInput;
LittleFSAdapter fileSystem;
Esp32Platform platform;

RoleFactory roleFactory(analogInput, nmea);
RoleManager roleManager(roleFactory, fileSystem);
DeviceInfo deviceInfo(platform);
BluetoothService bluetooth(&roleManager, &deviceInfo);

void setup() {
    Serial.begin(115200);

    if (!fileSystem.begin()) {
        Serial.println("LittleFS mount failed");
    }
    fileSystem.mkdir("/roles");

    // Start services
    nmea.start();
    bluetooth.start();

    // Load roles from filesystem and start all roles
    // TODO: Add error handling to role status
    loadRolesFromDirectory(roleManager, fileSystem, "/roles");
    roleManager.startAll();
}

void loop() {
    roleManager.loopAll();
    bluetooth.loop();
}
