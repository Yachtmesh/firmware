#include <Arduino.h>
#include "NMEA2000Service.h"
#include "FluidLevelSensorRole.h"
#include "BluetoothService.h"
#include "all.h"

Nmea2000Service nmea;
AnalogInputService analogInput;
FluidLevelSensorRole fluidLevelSensorRole(analogInput, nmea);
BluetoothService bluetooth;

void setup()
{
  Serial.begin(115200); // Start Serial
  nmea.start(115200);

  FluidLevelConfig cfg{FluidType::Water, 14, 257, 0.2f, 4.8f};
  fluidLevelSensorRole.configure(cfg);
  fluidLevelSensorRole.start();

  bluetooth.start();
  bluetooth.setRunningState(true);
  bluetooth.setHealthStatus(HealthStatus::Healthy);
}

void loop()
{
  fluidLevelSensorRole.loop();
  bluetooth.loop();
}