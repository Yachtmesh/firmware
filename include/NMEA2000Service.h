#pragma once
#include "all.h"
// #include <N2kTypes.h>

class Nmea2000ServiceInterface {
   public:
    virtual void sendMetric(const Metric& metric) = 0;
    virtual void start() = 0;

    virtual ~Nmea2000ServiceInterface() = default;
};

class Nmea2000Service : public Nmea2000ServiceInterface {
   public:
    void start() override;
    void sendMetric(const Metric& metric) override;

   private:
    int toN2kFluidType(FluidType t);
};