#include "LogStream.h"

#include <esp_log.h>

#include <cstring>

LogStream* LogStream::instance_ = nullptr;

void LogStream::install() {
    instance_ = this;
    queue_ = xQueueCreate(QUEUE_DEPTH, MAX_LINE_LEN);
    originalVprintf_ = esp_log_set_vprintf(hook);
}

void LogStream::setStreaming(bool enabled) {
    streaming_ = enabled;
    if (!enabled) {
        // Flush stale entries so the client starts fresh next time
        char buf[MAX_LINE_LEN];
        while (xQueueReceive(queue_, buf, 0) == pdTRUE) {
        }
    }
}

void LogStream::drainQueue(const std::function<void(const char*)>& cb) {
    char buf[MAX_LINE_LEN];
    while (xQueueReceive(queue_, buf, 0) == pdTRUE) {
        cb(buf);
    }
}

int LogStream::hook(const char* fmt, va_list args) {
    // Always forward to the original handler (serial output is unaffected)
    int ret = 0;
    if (instance_ && instance_->originalVprintf_) {
        va_list copy;
        va_copy(copy, args);
        ret = instance_->originalVprintf_(fmt, copy);
        va_end(copy);
    }

    if (!instance_ || !instance_->streaming_ || !instance_->queue_) {
        return ret;
    }

    char buf[MAX_LINE_LEN];
    vsnprintf(buf, sizeof(buf), fmt, args);

    // Strip trailing newline so the BLE client receives a clean line
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    // Non-blocking send — drop the line if the queue is full rather than stall
    xQueueSendFromISR(instance_->queue_, buf, nullptr);

    return ret;
}
