// #define N2k_SPI_CS_PIN 53    // If you use mcp_can and CS pin is not 53, uncomment this and modify definition to match your CS pin.
// #define N2k_CAN_INT_PIN 21   // If you use mcp_can and interrupt pin is not 21, uncomment this and modify definition to match your interrupt pin.
// #define USE_MCP_CAN_CLOCK_SET 8  // If you use mcp_can and your mcp_can shield has 8MHz chrystal, uncomment this.

#define ESP32_CAN_TX_PIN GPIO_NUM_5
#define ESP32_CAN_RX_PIN GPIO_NUM_4

#include "NMEA2000Service.h"
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>

const unsigned long TransmitMessages[] PROGMEM = {130310L, 130311L, 130312L, 0};

tN2kSyncScheduler TemperatureScheduler(false, 2000, 500);
tN2kSyncScheduler EnvironmentalScheduler(false, 500, 510);
tN2kSyncScheduler OutsideEnvironmentalScheduler(false, 500, 520);

// *****************************************************************************
double ReadCabinTemp()
{
    return CToKelvin(22.5); // Read here the true temperature e.g. from analog input
}

// *****************************************************************************
double ReadWaterTemp()
{
    return CToKelvin(15.5); // Read here the true temperature e.g. from analog input
}

// ****

// *****************************************************************************
void SendN2kTemperature()
{
    tN2kMsg N2kMsg;
    // Serial.print(millis()); Serial.println(", Sending message");
    if (TemperatureScheduler.IsTime())
    {
        TemperatureScheduler.UpdateNextTime();
        SetN2kTemperature(N2kMsg, 1, 1, N2kts_MainCabinTemperature, ReadCabinTemp());
        NMEA2000.SendMsg(N2kMsg);
    }

    if (EnvironmentalScheduler.IsTime())
    {
        EnvironmentalScheduler.UpdateNextTime();
        SetN2kEnvironmentalParameters(N2kMsg, 1, N2kts_MainCabinTemperature, ReadCabinTemp());
        NMEA2000.SendMsg(N2kMsg);
    }

    if (OutsideEnvironmentalScheduler.IsTime())
    {
        OutsideEnvironmentalScheduler.UpdateNextTime();
        double wt = ReadWaterTemp();
        SetN2kOutsideEnvironmentalParameters(N2kMsg, 1, wt);
        NMEA2000.SendMsg(N2kMsg);
        Serial.print(millis());
        Serial.println(", Temperature send ready, yess!");
    }
}

void OnN2kOpen()
{
    // Start schedulers now.
    TemperatureScheduler.UpdateNextTime();
    EnvironmentalScheduler.UpdateNextTime();
    OutsideEnvironmentalScheduler.UpdateNextTime();
}

// begin(): actually start hardware (Serial, CAN bus)
void Nmea2000Service::start(unsigned long serialBaud)
{

    // Set Product information
    NMEA2000.SetProductInformation(
        "00000001",              // Manufacturer's Model serial code
        100,                     // Manufacturer's product code
        "Simple temp monitor",   // Manufacturer's Model ID
        "1.2.0.21 (2022-09-30)", // Software version
        "1.1.0.0 (2022-09-30)"   // Model version
    );

    // Set device information
    NMEA2000.SetDeviceInformation(
        112233, // Unique number / Serial number
        130,    // Device function = Temperature
        75,     // Device class = Sensor Communication Interface
        2040    // Industry code / free from registry
    );

    // Configure CAN/NMEA mode
    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 22);

    // Forwarding and messages
    NMEA2000.SetForwardStream(&Serial);
    NMEA2000.EnableForward(false); // disable USB forwarding
    NMEA2000.ExtendTransmitMessages(TransmitMessages);

    // Set callback
    NMEA2000.SetOnOpen(OnN2kOpen);

    // Serial.begin(serialBaud); // Start Serial
    // Open NMEA2000 bus
    NMEA2000.Open();

    // Serial.println("NMEA Service ready");
}

// loop(): call this from Arduino loop
void Nmea2000Service::loop()
{
}

void Nmea2000Service::sendFluidLevel(float percent)
{
    SendN2kTemperature();     // your function to send PGNs
    NMEA2000.ParseMessages(); // parse incoming messages
}