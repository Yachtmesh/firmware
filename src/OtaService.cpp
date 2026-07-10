#include "OtaService.h"

#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_ota_ops.h>

static const char* TAG = "OtaService";

bool OtaService::begin(const std::string& url) {
    if (url.rfind("https://", 0) != 0) {
        lastError_ = "url must use https";
        return false;
    }
    url_ = url;

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = url_.c_str();
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
    httpConfig.keep_alive_enable = true;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;

    esp_err_t err = esp_https_ota_begin(&otaConfig, &handle_);
    if (err != ESP_OK) {
        lastError_ = esp_err_to_name(err);
        handle_ = nullptr;
        return false;
    }
    return true;
}

OtaPerformResult OtaService::perform() {
    esp_err_t err = esp_https_ota_perform(handle_);
    if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        return OtaPerformResult::InProgress;
    }
    if (err == ESP_OK && esp_https_ota_is_complete_data_received(handle_)) {
        return OtaPerformResult::Complete;
    }
    if (err == ESP_OK) {
        return OtaPerformResult::InProgress;
    }
    lastError_ = esp_err_to_name(err);
    return OtaPerformResult::Error;
}

bool OtaService::finish() {
    esp_err_t err = esp_https_ota_finish(handle_);
    handle_ = nullptr;
    if (err != ESP_OK) {
        lastError_ = esp_err_to_name(err);
        return false;
    }
    return true;
}

void OtaService::abort() {
    if (handle_) {
        esp_https_ota_abort(handle_);
        handle_ = nullptr;
    }
}

size_t OtaService::bytesRead() const {
    return handle_ ? esp_https_ota_get_image_len_read(handle_) : 0;
}

std::string OtaService::lastError() const { return lastError_; }

void OtaService::reboot() {
    ESP_LOGI(TAG, "Rebooting into new firmware image");
    esp_restart();
}
