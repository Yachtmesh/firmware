#pragma once
#include <string>

struct SerialReading {
    std::string line;
    bool valid;
};

class SerialSensorInterface {
   public:
    virtual SerialReading readLine() = 0;
    virtual ~SerialSensorInterface() = default;
};

#ifdef ESP32
#include <driver/uart.h>

class SerialSensorService : public SerialSensorInterface {
   public:
    SerialSensorService(uart_port_t port, int baudRate, int rxPin, int txPin);
    SerialReading readLine() override;

   private:
    uart_port_t port_;
    std::string lineBuffer_;
};
#endif
