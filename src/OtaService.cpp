#include "OtaService.h"

#include <cstring>

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
    httpConfig.buffer_size = 2048;
    httpConfig.buffer_size_tx = 2048;

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

bool OtaService::fetchManifest(const std::string& url, std::string& outBody) {
    outBody.clear();

    if (url.rfind("https://", 0) != 0) {
        lastError_ = "url must use https";
        return false;
    }

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = url.c_str();
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
    httpConfig.timeout_ms = MANIFEST_FETCH_TIMEOUT_MS;
    httpConfig.buffer_size = 2048;
    httpConfig.buffer_size_tx = 2048;

    esp_http_client_handle_t client = esp_http_client_init(&httpConfig);
    if (!client) {
        lastError_ = "failed to init http client";
        return false;
    }

    int contentLength = 0;
    for (int redirects = 0;; redirects++) {
        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            lastError_ = esp_err_to_name(err);
            esp_http_client_cleanup(client);
            return false;
        }

        contentLength = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        if (status >= 300 && status < 400) {
            esp_http_client_close(client);
            if (redirects >= MANIFEST_MAX_REDIRECTS ||
                esp_http_client_set_redirection(client) != ESP_OK) {
                lastError_ = "manifest too many redirects";
                esp_http_client_cleanup(client);
                return false;
            }
            char redirectUrl[256];
            esp_http_client_get_url(client, redirectUrl, sizeof(redirectUrl));
            if (strncmp(redirectUrl, "https://", 8) != 0) {
                lastError_ = "manifest redirected to non-https url";
                esp_http_client_cleanup(client);
                return false;
            }
            continue;
        }

        if (status < 200 || status >= 300) {
            lastError_ = "manifest http status " + std::to_string(status);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        break;
    }

    if (contentLength > 0 &&
        static_cast<size_t>(contentLength) > MANIFEST_MAX_BYTES) {
        lastError_ = "manifest too large";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    char buf[512];
    while (true) {
        int readLen = esp_http_client_read(client, buf, sizeof(buf));
        if (readLen < 0) {
            lastError_ = "manifest read error";
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        if (readLen == 0) break;

        if (outBody.size() + static_cast<size_t>(readLen) > MANIFEST_MAX_BYTES) {
            lastError_ = "manifest too large";
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        outBody.append(buf, readLen);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return true;
}
