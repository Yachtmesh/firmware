#pragma once

#include <cstdint>

struct VeDirectFrame {
    float voltage = 0.0f;     // V  (from V field, mV ÷ 1000)
    float current = 0.0f;     // A  (from I field, mA ÷ 1000)
    float soc = 0.0f;         // %  (from SOC field, ‰ ÷ 10)
    float power = 0.0f;       // W  (from P field)
    float ttg = -1.0f;        // minutes (-1 = unavailable, from TTG field)
    float consumedAh = 0.0f;  // Ah (from CE field, mAh ÷ -1000)
    bool checksumOk = false;  // true when block byte sum == 0 mod 256
    bool hasData = false;     // true when at least one battery field was decoded
};

class VeDirectParser {
   public:
    // Feed a key/value pair from one VE.Direct text line (without \r\n).
    // Returns true when a complete, checksum-validated frame is ready.
    // Call getFrame() immediately after a true return.
    bool feedLine(const char* key, const char* val);

    const VeDirectFrame& getFrame() const { return frame_; }

    void reset();

   private:
    VeDirectFrame pending_{};
    VeDirectFrame frame_{};
    uint8_t runningSum_ = 0;
};
