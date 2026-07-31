#include "AnalogInputManager.h"

AnalogInputManager::AnalogInputManager(I2cBusInterface& bus) : bus_(bus) {}

AnalogInputInterface* AnalogInputManager::claim(uint8_t address, uint8_t channel) {
    auto key = std::make_pair(address, channel);
    if (readers_.count(key)) {
        return nullptr;  // Already claimed
    }
    auto& chip = chips_[address];
    if (!chip) {
        chip = std::make_unique<Ads1115Chip>(bus_, address);
    }
    readers_[key] = std::make_unique<Ads1115ChannelReader>(*chip, channel);
    return readers_[key].get();
}

void AnalogInputManager::release(uint8_t address, uint8_t channel) {
    readers_.erase(std::make_pair(address, channel));
}
