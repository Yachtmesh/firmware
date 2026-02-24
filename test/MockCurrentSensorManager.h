#pragma once
#include <cstdint>

#include "CurrentSensorManager.h"
#include "CurrentSensorService.h"

class FakeCurrentSensor : public CurrentSensorInterface {
   public:
    CurrentSensorReading reading{0.0f, 0.0f, 0.0f, true};

    CurrentSensorReading read() override { return reading; }
};

class MockCurrentSensorManager : public CurrentSensorManagerInterface {
   public:
    FakeCurrentSensor sensor;
    bool claimCalled = false;
    bool releaseCalled = false;
    uint8_t lastClaimedAddress = 0;
    bool returnNull = false;  // Set true to simulate address conflict

    CurrentSensorInterface* claim(uint8_t address, float /*shuntOhms*/) override {
        claimCalled = true;
        lastClaimedAddress = address;
        if (returnNull) return nullptr;
        return &sensor;
    }

    void release(uint8_t /*address*/) override { releaseCalled = true; }
};
