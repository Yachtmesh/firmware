#pragma once
#include <cstdint>

#include "I2cBusService.h"

// Reading from an environmental sensor (BME280 implementation)
struct EnvironmentalReading {
    float temperature;  // Celsius
    float humidity;     // % relative humidity
    float pressure;     // hPa
    bool valid;
};

class EnvironmentalSensorInterface {
   public:
    virtual EnvironmentalReading read() = 0;
    virtual ~EnvironmentalSensorInterface() = default;
};

// EnvironmentalSensorService implements the BME280 I2C sensor.
// Only uses I2cBusInterface, so it is native-testable.
class EnvironmentalSensorService : public EnvironmentalSensorInterface {
   public:
    explicit EnvironmentalSensorService(I2cBusInterface& bus, uint8_t address = 0x76);
    EnvironmentalReading read() override;

   private:
    I2cBusInterface& bus_;
    uint8_t address_;
    bool initialized_ = false;
    int32_t t_fine_ = 0;  // Shared between temperature and pressure compensation

    struct CalData {
        uint16_t dig_T1;
        int16_t dig_T2, dig_T3;
        uint16_t dig_P1;
        int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
        uint8_t dig_H1;
        int16_t dig_H2;
        uint8_t dig_H3;
        int16_t dig_H4, dig_H5;
        int8_t dig_H6;
    } cal_;

    bool init();
    void loadCalibration();
    int32_t compensateTemp(int32_t adc_T);
    uint32_t compensatePressure(int32_t adc_P);
    uint32_t compensateHumidity(int32_t adc_H);
};
