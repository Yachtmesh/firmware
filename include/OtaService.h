#pragma once

#include <cstddef>
#include <string>

// Result of one non-blocking esp_https_ota_perform() step.
enum class OtaPerformResult { InProgress, Complete, Error };

// Abstract interface wrapping firmware OTA download/flash operations.
// Allows OtaManager to be tested without hardware dependencies.
class OtaServiceInterface {
   public:
    // Start a download from the given HTTPS URL. Returns false immediately
    // on obviously-invalid input (e.g. non-https URL) without touching flash.
    virtual bool begin(const std::string& url) = 0;

    // Advance the in-progress download by one non-blocking step. Call once
    // per loop tick until it returns Complete or Error.
    virtual OtaPerformResult perform() = 0;

    // Validate the fully-downloaded image and set it as the next boot
    // partition. Only valid after perform() returned Complete.
    virtual bool finish() = 0;

    // Abandon an in-progress or completed-but-unfinished download; does not
    // change the boot partition.
    virtual void abort() = 0;

    // Bytes of the image body received so far.
    virtual size_t bytesRead() const = 0;

    // Human-readable description of the last failing operation.
    virtual std::string lastError() const = 0;

    // Reboot into the newly-flashed partition.
    virtual void reboot() = 0;

    // Plain HTTPS GET of a small JSON body (the OTA manifest) — distinct from
    // begin()/perform(), which stream an ESP-IDF OTA binary image instead.
    // Returns false on network error, non-2xx status, or an oversized body.
    virtual bool fetchManifest(const std::string& url, std::string& outBody) = 0;

    virtual ~OtaServiceInterface() = default;
};

#ifdef ESP32

#include <esp_https_ota.h>
#include <esp_http_client.h>

// ESP32 implementation backed by ESP-IDF's non-blocking esp_https_ota API.
class OtaService : public OtaServiceInterface {
   public:
    bool begin(const std::string& url) override;
    OtaPerformResult perform() override;
    bool finish() override;
    void abort() override;
    size_t bytesRead() const override;
    std::string lastError() const override;
    void reboot() override;
    bool fetchManifest(const std::string& url, std::string& outBody) override;

    static constexpr uint32_t MANIFEST_FETCH_TIMEOUT_MS = 10000;
    static constexpr size_t MANIFEST_MAX_BYTES = 4096;
    static constexpr int MANIFEST_MAX_REDIRECTS = 5;

   private:
    esp_https_ota_handle_t handle_ = nullptr;
    std::string url_;
    std::string lastError_;
};

#endif
