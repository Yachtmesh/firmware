#pragma once
#include <string>

struct SerialReading {
    std::string line;
    bool valid;
};

class SerialSensorInterface {
   public:
    virtual void begin(int baudRate) = 0;
    virtual SerialReading readLine() = 0;
    virtual ~SerialSensorInterface() = default;
};

#ifdef ESP32
#include <driver/uart.h>

class SerialSensorService : public SerialSensorInterface {
   public:
    SerialSensorService(uart_port_t port, int rxPin, int txPin);
    void begin(int baudRate) override;
    SerialReading readLine() override;

   private:
    uart_port_t port_;
    int rxPin_;
    int txPin_;
    std::string lineBuffer_;
};
#endif
