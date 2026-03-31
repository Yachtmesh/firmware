#pragma once
#include <queue>
#include <string>

#include "SerialSensorService.h"

class MockSerialSensorService : public SerialSensorInterface {
   public:
    std::queue<std::string> lines;

    void enqueue(const std::string& line) { lines.push(line); }

    void begin(int) override {}

    SerialReading readLine() override {
        if (lines.empty()) return {"", false};
        SerialReading r{lines.front(), true};
        lines.pop();
        return r;
    }
};
