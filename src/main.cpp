#include <Arduino.h>
#include <LittleFS.h>

#include "BluetoothService.h"
#include "LittleFSAdapter.h"
#include "NMEA2000Service.h"
#include "RoleFactory.h"
#include "RoleManager.h"

Nmea2000Service nmea;
AnalogInputService analogInput;
LittleFSAdapter fileSystem;

RoleFactory roleFactory(analogInput, nmea);
RoleManager roleManager(roleFactory, fileSystem);
BluetoothService bluetooth(&roleManager);

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {  // true = format if mount fails
        Serial.println("LittleFS mount failed");
    }

    nmea.start(115200);
    bluetooth.start();

    // Load all roles from /roles/ directory
    roleManager.loadFromDirectory("/roles");
    roleManager.startAll();
}

void loop() {
    roleManager.loopAll();
    bluetooth.loop();
}
