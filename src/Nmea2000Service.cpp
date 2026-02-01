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

const unsigned long TransmitMessages[] PROGMEM = {130310L, 130311L, 130312L, 0};

tN2kSyncScheduler FluidLevelScheduler(false, 2000, 500);
tN2kSyncScheduler EnvironmentalScheduler(false, 500, 510);
tN2kSyncScheduler OutsideEnvironmentalScheduler(false, 500, 520);

void OnN2kOpen() {
    // Start schedulers now.
    FluidLevelScheduler.UpdateNextTime();
    EnvironmentalScheduler.UpdateNextTime();
    OutsideEnvironmentalScheduler.UpdateNextTime();
}

// begin(): actually start hardware (Serial, CAN bus)
void Nmea2000Service::start() {
    // Set Product information
    NMEA2000.SetProductInformation(
        "00000001",               // Manufacturer's Model serial code
        100,                      // Manufacturer's product code
        "Simple temp monitor",    // Manufacturer's Model ID
        "1.2.0.21 (2022-09-30)",  // Software version
        "1.1.0.0 (2022-09-30)"    // Model version
    );

    // Set device information
    NMEA2000.SetDeviceInformation(
        112233,  // Unique number / Serial number
        130,     // Device function = Temperature
        75,      // Device class = Sensor Communication Interface
        2040     // Industry code / free from registry
    );

    // Configure CAN/NMEA mode
    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 22);

    // Forwarding and messages
    NMEA2000.SetForwardStream(&Serial);
    NMEA2000.EnableForward(false);  // disable USB forwarding
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
            FluidLevelScheduler.UpdateNextTime();

            tN2kFluidType fluidType = static_cast<tN2kFluidType>(
                toN2kFluidType(metric.context.fluidLevel.fluidType));
            uint16_t capacity = metric.context.fluidLevel.capacity;
            unsigned char inst = metric.context.fluidLevel.inst;

            SetN2kFluidLevel(N2kMsg, inst, fluidType,
                             static_cast<double>(metric.value), capacity);

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