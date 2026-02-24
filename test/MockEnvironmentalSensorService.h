#pragma once
#include "EnvironmentalSensorService.h"

class MockEnvironmentalSensorService : public EnvironmentalSensorInterface {
   public:
    EnvironmentalReading reading{20.0f, 50.0f, 1013.25f, true};

    EnvironmentalReading read() override { return reading; }
};
