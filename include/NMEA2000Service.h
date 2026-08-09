#pragma once
#include <vector>

#include "types.h"

struct FluidLevelContext {
    unsigned char inst;   // maps to instance, e.g. tank number
    FluidType fluidType;  // maps to NMEA2000::N2kFluidType
    int16_t capacity;     // tank capacity
};

struct EnvironmentalContext {
    uint8_t inst;        // NMEA2000 instance number
    float temperature;   // degrees Celsius
    float humidity;      // % relative humidity
    float pressure;      // hPa
};

struct BatteryContext {
    uint8_t inst;         // NMEA2000 battery instance number
    float voltage;        // V
    float current;        // A
    float soc;            // % state of charge
    float ttg;            // minutes remaining (-1 = unavailable)
    float consumedAh;     // Ah consumed
    float temperature;    // °C, Battery Case Temperature (AUX configured as Temperature)
    bool hasTemperature;
    float auxVoltage;     // V, second/starter or mid-point battery voltage (AUX configured as Starter/MidPoint)
    bool hasAuxVoltage;
};

enum class MetricType : uint8_t {
    FluidLevel,
    Environmental,
    BatteryStatus,
};

struct MetricContext {
    union {
        FluidLevelContext fluidLevel;
        EnvironmentalContext environmental;
        BatteryContext battery;
    };
};

struct Metric {
    MetricType type;
    float value;
    uint8_t priority = 3;
    MetricContext context;

    Metric(MetricType t, float v, uint8_t inst = 0, uint8_t prio = 3)
        : type(t), value(v), priority(prio) {}
};

// Listener interface for raw N2K messages
class N2kListenerInterface {
   public:
    virtual void onN2kMessage(uint32_t pgn, uint8_t priority, uint8_t source,
                              const unsigned char* data, size_t len) = 0;
    virtual ~N2kListenerInterface() = default;
};

class Nmea2000ServiceInterface {
   public:
    virtual void sendMetric(const Metric& metric) = 0;
    virtual void sendMsg(uint32_t pgn, uint8_t priority,
                         const unsigned char* data, size_t len) {}
    virtual void start() = 0;
    virtual void addListener(N2kListenerInterface*) {}
    virtual void removeListener(N2kListenerInterface*) {}
    virtual void loop() {}
    virtual uint8_t getAddress() const { return 255; }

    virtual ~Nmea2000ServiceInterface() = default;
};

// Derive the NMEA2000 "unique number" (21 bits, 0-2097151) from a 6-byte MAC
// address, combined with Yachtmesh's NMEA2000 industry code (2040). Pure
// function of the MAC so callers can resolve it once alongside the device ID
// (see DeviceId.h) from the same MAC read.
uint32_t nmea2000UniqueNumberFromMac(const uint8_t mac[6]);

#ifdef ESP32
#include <N2kMsg.h>

#include <string>

class Nmea2000Service : public Nmea2000ServiceInterface {
   public:
    // Derives the device ID (for the Model ID, shown alongside the same ID
    // in the BLE advertised name) and the NMEA2000 unique number from the
    // given MAC. Caller resolves the MAC (see PlatformInterface) and must
    // call this before start(); this class owns how NMEA2000-specific
    // identity fields are built from it.
    void setIdentity(const uint8_t mac[6]);

    void start() override;
    void sendMetric(const Metric& metric) override;
    void sendMsg(uint32_t pgn, uint8_t priority, const unsigned char* data,
                 size_t len) override;
    void addListener(N2kListenerInterface* listener) override;
    void removeListener(N2kListenerInterface* listener) override;
    void loop() override;
    uint8_t getAddress() const override;

   private:
    void notifyListeners(const tN2kMsg& msg);
    int toN2kFluidType(FluidType t);
    std::string deviceId_;
    uint32_t uniqueNumber_ = 0;
    std::vector<N2kListenerInterface*> listeners_;
    uint8_t lastLoggedAddress_ = 22;  // matches preferred address in SetMode()
};
#endif
