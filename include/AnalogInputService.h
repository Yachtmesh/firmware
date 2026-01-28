#pragma once
#include <string>

class AnalogInputInterface {
   public:
    virtual float readVoltage() = 0;
    virtual ~AnalogInputInterface() = default;
};

class AnalogInputService : public AnalogInputInterface {
   public:
    float readVoltage() override;
};

class FluidLevelCalculator {
   public:
    FluidLevelCalculator(float minV, float maxV);

    float toPercent(float voltage) const;

   private:
    float minV;
    float maxV;
};
