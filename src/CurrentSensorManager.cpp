#include "CurrentSensorManager.h"

CurrentSensorManager::CurrentSensorManager(I2cBusInterface& bus) : bus_(bus) {}

CurrentSensorInterface* CurrentSensorManager::claim(uint8_t address, float shuntOhms) {
    if (sensors_.count(address)) {
        return nullptr;  // Already claimed
    }
    sensors_[address] = std::make_unique<CurrentSensorService>(bus_, address, shuntOhms);
    return sensors_[address].get();
}

void CurrentSensorManager::release(uint8_t address) {
    sensors_.erase(address);
}
