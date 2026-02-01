#include "AnalogInputService.h"

#include <Arduino.h>

float AnalogInputService::readVoltage() {
    const int adcPin = 34;
    int adcValue = analogRead(adcPin);

    // Convert to voltage
    float voltage = adcValue * (3.3 / 4095.0);

    return voltage;
}