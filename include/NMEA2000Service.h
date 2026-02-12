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

// Listener interface for raw N2K messages (uses primitives — native-testable)
class N2kListenerInterface {
   public:
    virtual void onN2kMessage(unsigned long pgn, unsigned char source,
                              unsigned char priority, int dataLen,
                              const unsigned char* data,
                              unsigned long msgTime) = 0;
    virtual ~N2kListenerInterface() = default;
};

class Nmea2000ServiceInterface {
   public:
    virtual void sendMetric(const Metric& metric) = 0;
    virtual void start() = 0;
    virtual void addListener(N2kListenerInterface*) {}
    virtual void removeListener(N2kListenerInterface*) {}
    virtual void loop() {}

    virtual ~Nmea2000ServiceInterface() = default;
};

class Nmea2000Service : public Nmea2000ServiceInterface {
   public:
    void start() override;
    void sendMetric(const Metric& metric) override;
    void addListener(N2kListenerInterface* listener) override;
    void removeListener(N2kListenerInterface* listener) override;
    void loop() override;

   private:
    int toN2kFluidType(FluidType t);
    std::vector<N2kListenerInterface*> listeners_;
};
