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

    bool fetchManifestResult = true;
    // Resolves to "esp32dev" -> "fw.bin", joined against a manifestUrl whose
    // directory is itself "https://example.com/" -- matches MockPlatform's
    // default board and keeps existing tests' "https://example.com/fw.bin"
    // lastBeginUrl assertions unchanged unless a test overrides this.
    std::string manifestBody =
        R"({"version":"v1.4.0","targets":{"esp32dev":{"file":"fw.bin","sha256":"abc123","sizeBytes":1000}}})";
    std::string fetchManifestErrorMessage = "manifest fetch error";
    std::string lastFetchManifestUrl;
    int fetchManifestCallCount = 0;

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

    bool fetchManifest(const std::string& url, std::string& outBody) override {
        fetchManifestCallCount++;
        lastFetchManifestUrl = url;
        if (!fetchManifestResult) {
            errorMessage = fetchManifestErrorMessage;
            return false;
        }
        outBody = manifestBody;
        return true;
    }
};
