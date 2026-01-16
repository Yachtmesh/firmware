#pragma once
#include "all.h"

class Nmea2000ServiceInterface
{
public:
    virtual void sendFluidLevel(float percent) = 0;
    virtual void sendMetric(const Metric &metric) = 0;
    virtual void start(unsigned long serialBaud) = 0;
    virtual void loop() = 0;

    virtual ~Nmea2000ServiceInterface() = default;
};

class Nmea2000Service : public Nmea2000ServiceInterface
{
public:
    void start(unsigned long serialBaud) override;
    void loop() override;
    void sendFluidLevel(float percent) override;
    void sendMetric(const Metric &metric) override;
};