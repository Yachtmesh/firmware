// #define N2k_SPI_CS_PIN 53    // If you use mcp_can and CS pin is not 53,
// uncomment this and modify definition to match your CS pin. #define
// N2k_CAN_INT_PIN 21   // If you use mcp_can and interrupt pin is not 21,
// uncomment this and modify definition to match your interrupt pin. #define
// USE_MCP_CAN_CLOCK_SET 8  // If you use mcp_can and your mcp_can shield has
// 8MHz chrystal, uncomment this.

#define ESP32_CAN_TX_PIN GPIO_NUM_5
#define ESP32_CAN_RX_PIN GPIO_NUM_4

#include "NMEA2000Service.h"

#include <N2KMessages.h>
#include <NMEA2000_CAN.h>
#include <esp_mac.h>

const unsigned long TransmitMessages[] PROGMEM = {130310L, 130311L, 130312L, 0};
const uint32_t kIndustryCode = 2040;

tN2kSyncScheduler FluidLevelScheduler(false, 2000, 500);

void OnN2kOpen() {
    // Start schedulers now.
    FluidLevelScheduler.UpdateNextTime();
}

// Generate unique number from industry code 2040 and ESP32 chip ID
// NMEA2000 unique number is 21 bits (0-2097151)
static uint32_t generateUniqueNumber() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    uint64_t chipId = ((uint64_t)mac[0] << 40) | ((uint64_t)mac[1] << 32) |
                      ((uint64_t)mac[2] << 24) | ((uint64_t)mac[3] << 16) |
                      ((uint64_t)mac[4] << 8) | mac[5];
    // Use industry code as base, add lower bits of chip ID for uniqueness
    // 2040 * 1000 = 2,040,000 leaves room for ~57,000 devices within 21-bit
    // limit
    uint32_t uniqueNumber = (kIndustryCode * 1000) + (chipId & 0xFFFF) % 57000;
    return uniqueNumber & 0x1FFFFF;  // Mask to 21 bits
}

// begin(): actually start hardware (Serial, CAN bus)
void Nmea2000Service::start() {
    // Set Product information
    NMEA2000.SetProductInformation(
        "00000001",   // Manufacturer's Model serial code
        100,          // Manufacturer's product code
        "Yachtmesh",  // Manufacturer's Model ID
        "0.0.1",      // Software version
        "1"           // Model version
    );

    // Set device information
    NMEA2000.SetDeviceInformation(
        generateUniqueNumber(),  // Unique number derived from ESP32 chip ID
        130,                     // Device function = Temperature
        75,            // Device class = Sensor Communication Interface
        kIndustryCode  // Industry code / manufacturer ID
    );

    // Configure CAN/NMEA mode
    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 22);

    // Disable USB forwarding
    NMEA2000.SetForwardStream(nullptr);
    NMEA2000.EnableForward(false);
    NMEA2000.ExtendTransmitMessages(TransmitMessages);

    // Set callback
    NMEA2000.SetOnOpen(OnN2kOpen);

    // Open NMEA2000 bus
    NMEA2000.Open();
}

void Nmea2000Service::sendMetric(const Metric& metric) {
    tN2kMsg N2kMsg;

    switch (metric.type) {
        case MetricType::FluidLevel: {
            if (FluidLevelScheduler.IsTime()) {
                tN2kFluidType fluidType = static_cast<tN2kFluidType>(
                    toN2kFluidType(metric.context.fluidLevel.fluidType));
                uint16_t capacity = metric.context.fluidLevel.capacity;
                unsigned char inst = metric.context.fluidLevel.inst;

                SetN2kFluidLevel(N2kMsg, inst, fluidType,
                                 static_cast<double>(metric.value), capacity);

                FluidLevelScheduler.UpdateNextTime();
            }
            break;
        }

        default:
            // Nothing to send, return
            return;
    }

    NMEA2000.SendMsg(N2kMsg);
    NMEA2000.ParseMessages();  // parse incoming messages
}

int Nmea2000Service::toN2kFluidType(FluidType t) {
    switch (t) {
        case FluidType::Fuel:
            return tN2kFluidType::N2kft_Fuel;
        case FluidType::Water:
            return tN2kFluidType::N2kft_Water;
        case FluidType::GrayWater:
            return tN2kFluidType::N2kft_GrayWater;
        case FluidType::LiveWell:
            return tN2kFluidType::N2kft_LiveWell;
        case FluidType::Oil:
            return tN2kFluidType::N2kft_Oil;
        case FluidType::BlackWater:
            return tN2kFluidType::N2kft_BlackWater;
        case FluidType::FuelGasoline:
            return tN2kFluidType::N2kft_FuelGasoline;
        case FluidType::Error:
            return tN2kFluidType::N2kft_Error;
        default:
            return tN2kFluidType::N2kft_Unavailable;
    }
};
