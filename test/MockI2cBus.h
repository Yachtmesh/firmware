#pragma once
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include "I2cBusService.h"

class MockI2cBus : public I2cBusInterface {
   public:
    struct WriteCall {
        uint8_t addr;
        uint8_t reg;
        std::vector<uint8_t> data;
    };

    std::vector<WriteCall> writes;

    // Pre-load a 16-bit register value for reading
    void setRegister(uint8_t addr, uint8_t reg, uint16_t value) {
        registerData_[{addr, reg}] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    }

    void setRegisterByte(uint8_t addr, uint8_t reg, uint8_t value) {
        registerData_[{addr, reg}] = {value};
    }

    void setRegisterBytes(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t len) {
        registerData_[{addr, reg}] = std::vector<uint8_t>(buf, buf + len);
    }

    bool readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) override {
        auto key = std::make_pair(addr, reg);
        auto it = registerData_.find(key);
        if (it == registerData_.end() || it->second.size() < len) {
            memset(buf, 0, len);
            return true;
        }
        memcpy(buf, it->second.data(), len);
        return true;
    }

    bool writeBytes(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t len) override {
        writes.push_back({addr, reg, std::vector<uint8_t>(buf, buf + len)});
        registerData_[{addr, reg}] = std::vector<uint8_t>(buf, buf + len);
        return true;
    }

   private:
    std::map<std::pair<uint8_t, uint8_t>, std::vector<uint8_t>> registerData_;
};
