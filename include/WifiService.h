#pragma once

#include <string>

class WifiServiceInterface {
   public:
    virtual bool connect(const char* ssid, const char* password) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string ipAddress() const = 0;
    virtual ~WifiServiceInterface() = default;
};

#ifdef ESP32
#include <esp_event.h>
#include <esp_wifi.h>

class WifiService : public WifiServiceInterface {
   public:
    bool connect(const char* ssid, const char* password) override;
    void disconnect() override;
    bool isConnected() const override;
    std::string ipAddress() const override;

   private:
    void initWifi();

    static void eventHandler(void* arg, esp_event_base_t eventBase,
                             int32_t eventId, void* eventData);

    bool initialized_ = false;
    bool started_ = false;  // true between esp_wifi_start() and esp_wifi_stop()
    volatile bool connected_ = false;
    int refCount_ = 0;  // Number of active users; actual disconnect at zero
    char ipAddress_[16] = "";
};
#endif
