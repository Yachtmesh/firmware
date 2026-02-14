#pragma once
#include <vector>

#include "types.h"

struct FluidLevelContext {
    unsigned char inst;   // maps to instance, e.g. tank number
    FluidType fluidType;  // maps to NMEA2000::N2kFluidType
    int16_t capacity;     // tank capacity
};

enum class MetricType : uint8_t {
    FluidLevel,
};

struct MetricContext {
    union {
        FluidLevelContext fluidLevel;
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

// Listener interface for pre-encoded N2K data (Actisense binary frames)
class N2kListenerInterface {
   public:
    virtual void onN2kData(const unsigned char* data, size_t len) = 0;
    virtual ~N2kListenerInterface() = default;
};

class Nmea2000ServiceInterface {
   public:
    virtual void sendMetric(const Metric& metric) = 0;
    virtual void start() = 0;
    virtual void addListener(N2kListenerInterface*) {}
    virtual void removeListener(N2kListenerInterface*) {}
    virtual void loop() {}
    virtual uint8_t getAddress() const { return 255; }

    virtual ~Nmea2000ServiceInterface() = default;
};

#ifdef ESP32
#include <N2kMsg.h>

class Nmea2000Service : public Nmea2000ServiceInterface {
   public:
    void start() override;
    void sendMetric(const Metric& metric) override;
    void addListener(N2kListenerInterface* listener) override;
    void removeListener(N2kListenerInterface* listener) override;
    void loop() override;
    uint8_t getAddress() const override;

   private:
    void notifyListeners(const tN2kMsg& msg);
    int toN2kFluidType(FluidType t);
    std::vector<N2kListenerInterface*> listeners_;
};
#endif
