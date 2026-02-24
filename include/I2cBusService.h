#pragma once
#include <cstddef>
#include <cstdint>

class I2cBusInterface {
   public:
    virtual bool readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) = 0;
    virtual bool writeBytes(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t len) = 0;
    virtual ~I2cBusInterface() = default;
};

#ifdef ESP32
#include <driver/i2c_master.h>

#include <map>

class Esp32I2cBus : public I2cBusInterface {
   public:
    Esp32I2cBus(int sdaPin, int sclPin, i2c_port_t port = I2C_NUM_0);
    ~Esp32I2cBus();
    bool readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeBytes(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t len) override;

   private:
    i2c_master_bus_handle_t busHandle_;
    std::map<uint8_t, i2c_master_dev_handle_t> devices_;
    i2c_master_dev_handle_t getOrAddDevice(uint8_t addr);
};
#endif
