#include "AnalogInputService.h"

#include <driver/adc.h>
#include <esp_adc_cal.h>

static const adc1_channel_t ADC_CHANNEL = ADC1_CHANNEL_6;  // GPIO 34
static const adc_atten_t ADC_ATTEN = ADC_ATTEN_DB_12;
static const adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;

static bool adcInitialized = false;
static esp_adc_cal_characteristics_t adcChars;

static void ensureAdcInitialized() {
    if (adcInitialized) return;

    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adcChars);

    adcInitialized = true;
}

float AnalogInputService::readVoltage() {
    ensureAdcInitialized();

    int adcValue = adc1_get_raw(ADC_CHANNEL);

    uint32_t millivolts = esp_adc_cal_raw_to_voltage(adcValue, &adcChars);

    return millivolts / 1000.0f;
}
