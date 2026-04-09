#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdio>
#include <functional>

// LogStream intercepts ESP-IDF log output and forwards lines to a registered
// callback when streaming is enabled. The vprintf hook is safe to call from
// any task or ISR context — it enqueues lines non-blocking and never stalls.
// The caller drains the queue from the main loop via drainQueue().
class LogStream {
   public:
    static constexpr size_t MAX_LINE_LEN = 120;
    static constexpr size_t QUEUE_DEPTH = 32;

    // Install the vprintf hook. Call once at boot before any logging occurs.
    void install();

    // Enable or disable forwarding log lines into the queue.
    void setStreaming(bool enabled);
    bool isStreaming() const { return streaming_; }

    // Drain all queued log lines, calling cb for each one. Call from main loop.
    void drainQueue(const std::function<void(const char*)>& cb);

   private:
    static int hook(const char* fmt, va_list args);

    static LogStream* instance_;
    vprintf_like_t originalVprintf_ = nullptr;
    QueueHandle_t queue_ = nullptr;
    bool streaming_ = false;
};
