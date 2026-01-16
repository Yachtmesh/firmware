#include <Arduino.h>
#include "NMEA2000Service.h"
#include "FluidLevelSensorRole.h"
#include "all.h"

Nmea2000Service nmea;
AnalogInput analogInput;
FluidLevelSensorRole fluidLevelSensorRole(analogInput, nmea);

void setup()
{
  Serial.begin(115200); // Start Serial
  nmea.start(115200);
  
  FluidLevelConfig cfg{0.2f, 4.8f};
  fluidLevelSensorRole.configure(cfg);
  fluidLevelSensorRole.start();
}

void loop()
{
  // nmea.loop();
  fluidLevelSensorRole.loop();
  Serial.println("i am looping!");
}