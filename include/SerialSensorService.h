#pragma once
#include <string>

struct SerialReading {
    std::string line;
    bool valid;
};

class SerialSensorInterface {
   public:
    virtual void begin(int baudRate, int rxPin, int txPin) = 0;
    virtual SerialReading readLine() = 0;
    virtual ~SerialSensorInterface() = default;
};

#ifdef ESP32
#include <driver/uart.h>

class SerialSensorService : public SerialSensorInterface {
   public:
    explicit SerialSensorService(uart_port_t port);
    void begin(int baudRate, int rxPin, int txPin) override;
    SerialReading readLine() override;

   private:
    uart_port_t port_;
    std::string lineBuffer_;
};
#endif
