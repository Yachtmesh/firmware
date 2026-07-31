#pragma once
#include <cstdint>

#include "AnalogInputManager.h"
#include "AnalogInputService.h"

class FakeAnalogInput : public AnalogInputInterface {
   public:
    AnalogInputReading reading{0.0f, true};

    AnalogInputReading read() override { return reading; }
};

class MockAnalogInputManager : public AnalogInputManagerInterface {
   public:
    FakeAnalogInput sensor;
    bool claimCalled = false;
    bool releaseCalled = false;
    uint8_t lastClaimedAddress = 0;
    uint8_t lastClaimedChannel = 0;
    bool returnNull = false;  // Set true to simulate address/channel conflict

    AnalogInputInterface* claim(uint8_t address, uint8_t channel) override {
        claimCalled = true;
        lastClaimedAddress = address;
        lastClaimedChannel = channel;
        if (returnNull) return nullptr;
        return &sensor;
    }

    void release(uint8_t /*address*/, uint8_t /*channel*/) override { releaseCalled = true; }
};
