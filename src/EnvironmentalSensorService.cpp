#include "EnvironmentalSensorService.h"

// BME280 register addresses (datasheet §5.3)
static constexpr uint8_t REG_CHIP_ID   = 0xD0;
static constexpr uint8_t REG_RESET     = 0xE0;
static constexpr uint8_t REG_CTRL_HUM  = 0xF2;
static constexpr uint8_t REG_STATUS    = 0xF3;
static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
static constexpr uint8_t REG_CONFIG    = 0xF5;
static constexpr uint8_t REG_DATA      = 0xF7;

// Calibration register addresses (datasheet §4.2.2)
static constexpr uint8_t REG_CAL_TP  = 0x88;  // Temperature + pressure cal (24 bytes)
static constexpr uint8_t REG_CAL_H1  = 0xA1;  // Humidity H1 (1 byte)
static constexpr uint8_t REG_CAL_H2  = 0xE1;  // Humidity H2–H6 (7 bytes)

static constexpr uint8_t BME280_CHIP_ID    = 0x60;  // Expected chip ID
static constexpr uint8_t BME280_RESET_CMD  = 0xB6;  // Soft-reset command (datasheet §5.4.2)

// ctrl_hum register: humidity oversampling ×1 (bits [2:0] = 001)
static constexpr uint8_t BME280_OSRS_H_1X = 0x01;

// ctrl_meas register: temp ×1 (bits[7:5]=001), pressure ×1 (bits[4:2]=001), normal mode (bits[1:0]=11)
static constexpr uint8_t BME280_CTRL_MEAS_DEFAULT = 0x27;

// Nibble mask for unpacking H4/H5 calibration words
static constexpr uint8_t NIBBLE_MASK = 0x0F;

// Output scaling: compensation functions return fixed-point values (Bosch datasheet §4.2.3)
//   compensateTemp     → 0.01 °C units  → divide by 100
//   compensateHumidity → Q22.10 format  → divide by 1024
//   compensatePressure → Pa × 256       → divide by 256, then by 100 for hPa
static constexpr float TEMP_SCALE      = 100.0f;
static constexpr float HUMIDITY_SCALE  = 1024.0f;
static constexpr float PRESSURE_SCALE  = 256.0f * 100.0f;  // Pa×256 → hPa

// Humidity clamp: 100% RH in Q22.10 = 100 × 2^22 = 419430400
static constexpr uint32_t HUMIDITY_MAX_Q22_10 = 419430400;

EnvironmentalSensorService::EnvironmentalSensorService(I2cBusInterface& bus, uint8_t address)
    : bus_(bus), address_(address), cal_{} {}

bool EnvironmentalSensorService::init() {
    uint8_t id = 0;
    if (!bus_.readBytes(address_, REG_CHIP_ID, &id, 1)) return false;
    if (id != BME280_CHIP_ID) return false;

    uint8_t reset = BME280_RESET_CMD;
    bus_.writeBytes(address_, REG_RESET, &reset, 1);

    loadCalibration();

    uint8_t hum = BME280_OSRS_H_1X;
    bus_.writeBytes(address_, REG_CTRL_HUM, &hum, 1);
    uint8_t meas = BME280_CTRL_MEAS_DEFAULT;
    bus_.writeBytes(address_, REG_CTRL_MEAS, &meas, 1);

    initialized_ = true;
    return true;
}

void EnvironmentalSensorService::loadCalibration() {
    uint8_t buf[26] = {};
    bus_.readBytes(address_, REG_CAL_TP, buf, 24);

    cal_.dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    cal_.dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    cal_.dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);
    cal_.dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    cal_.dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    cal_.dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    cal_.dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    cal_.dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    cal_.dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    cal_.dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    cal_.dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    cal_.dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);

    bus_.readBytes(address_, REG_CAL_H1, &cal_.dig_H1, 1);

    uint8_t h[7] = {};
    bus_.readBytes(address_, REG_CAL_H2, h, 7);
    cal_.dig_H2 = (int16_t)(h[1] << 8 | h[0]);
    cal_.dig_H3 = h[2];
    cal_.dig_H4 = (int16_t)((h[3] << 4) | (h[4] & NIBBLE_MASK));
    cal_.dig_H5 = (int16_t)((h[5] << 4) | (h[4] >> 4));
    cal_.dig_H6 = (int8_t)h[6];
}

// The three compensation functions below are verbatim ports of the Bosch reference
// integer algorithm from BME280 datasheet BST-BME280-DS002, §4.2.3 "Compensation
// formulas in double precision floating point". All shift amounts, additive offsets
// (76800, 128000, 16384, 32768, …) and multiplicative constants (3125, 2097152, …)
// are fixed-point arithmetic constants defined by that algorithm — not sensor-specific
// calibration values — so they are intentionally left as literals here.

int32_t EnvironmentalSensorService::compensateTemp(int32_t adc_T) {
    int32_t var1 = ((adc_T >> 3) - ((int32_t)cal_.dig_T1 << 1));
    var1 = (var1 * (int32_t)cal_.dig_T2) >> 11;
    int32_t var2 = (adc_T >> 4) - (int32_t)cal_.dig_T1;
    var2 = (((var2 * var2) >> 12) * (int32_t)cal_.dig_T3) >> 14;
    t_fine_ = var1 + var2;
    return (t_fine_ * 5 + 128) >> 8;  // 0.01 °C units
}

uint32_t EnvironmentalSensorService::compensatePressure(int32_t adc_P) {
    int64_t var1 = (int64_t)t_fine_ - 128000;
    int64_t var2 = var1 * var1 * (int64_t)cal_.dig_P6;
    var2 = var2 + ((var1 * (int64_t)cal_.dig_P5) << 17);
    var2 = var2 + ((int64_t)cal_.dig_P4 << 35);
    var1 = ((var1 * var1 * (int64_t)cal_.dig_P3) >> 8) + ((var1 * (int64_t)cal_.dig_P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * (int64_t)cal_.dig_P1 >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)cal_.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)cal_.dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)cal_.dig_P7 << 4);
    return (uint32_t)p;  // Pa * 256
}

uint32_t EnvironmentalSensorService::compensateHumidity(int32_t adc_H) {
    int32_t v = t_fine_ - (int32_t)76800;
    v = (((adc_H << 14) - ((int32_t)cal_.dig_H4 << 20) - ((int32_t)cal_.dig_H5 * v) + 16384) >> 15) *
        ((((((v * (int32_t)cal_.dig_H6) >> 10) *
            (((v * (int32_t)cal_.dig_H3) >> 11) + 32768)) >>
           10) +
          2097152) *
             (int32_t)cal_.dig_H2 +
         8192) >>
        14;
    v -= (((v >> 15) * (v >> 15) * (int32_t)cal_.dig_H1) >> 7) - (16384 >> 4);
    if (v < 0) v = 0;
    if (v > (int32_t)HUMIDITY_MAX_Q22_10) v = (int32_t)HUMIDITY_MAX_Q22_10;
    return (uint32_t)(v >> 12);  // Q22.10 fixed point
}

EnvironmentalReading EnvironmentalSensorService::read() {
    if (!initialized_) {
        if (!init()) return {0, 0, 0, false};
    }

    // Read 8 bytes: press[2:0], temp[2:0], hum[1:0]
    uint8_t data[8] = {};
    if (!bus_.readBytes(address_, REG_DATA, data, 8)) return {0, 0, 0, false};

    int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
    int32_t adc_H = ((int32_t)data[6] << 8) | data[7];

    int32_t tRaw = compensateTemp(adc_T);
    uint32_t pRaw = compensatePressure(adc_P);
    uint32_t hRaw = compensateHumidity(adc_H);

    return {
        (float)tRaw / TEMP_SCALE,
        (float)hRaw / HUMIDITY_SCALE,
        (float)pRaw / PRESSURE_SCALE,
        true,
    };
}
