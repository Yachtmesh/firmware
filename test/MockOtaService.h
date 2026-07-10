#pragma once

#include <string>
#include <vector>

#include "OtaService.h"

// Mock OTA service for testing OtaManager's state machine without hardware.
class MockOtaService : public OtaServiceInterface {
   public:
    bool beginResult = true;
    std::string errorMessage;
    std::vector<OtaPerformResult> performResults;
    size_t performIndex = 0;
    size_t bytesReadValue = 0;
    bool finishResult = true;
    bool abortCalled = false;
    bool rebootCalled = false;
    std::string lastBeginUrl;
    int beginCallCount = 0;
    int finishCallCount = 0;

    bool begin(const std::string& url) override {
        beginCallCount++;
        lastBeginUrl = url;
        return beginResult;
    }

    OtaPerformResult perform() override {
        if (performIndex < performResults.size()) {
            return performResults[performIndex++];
        }
        return OtaPerformResult::InProgress;
    }

    bool finish() override {
        finishCallCount++;
        return finishResult;
    }

    void abort() override { abortCalled = true; }

    size_t bytesRead() const override { return bytesReadValue; }

    std::string lastError() const override { return errorMessage; }

    void reboot() override { rebootCalled = true; }
};
