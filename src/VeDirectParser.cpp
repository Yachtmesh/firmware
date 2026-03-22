#include "VeDirectParser.h"

#include <cstdlib>
#include <cstring>

void VeDirectParser::reset() {
    pending_ = {};
    runningSum_ = 0;
}

bool VeDirectParser::feedLine(const char* key, const char* val) {
    // Replicate the byte sum that the device computed over the raw serial stream.
    // readLine() strips \r\n so we add them back here to keep the running checksum correct.
    for (const char* p = key; *p; p++) runningSum_ += (uint8_t)*p;
    runningSum_ += '\t';
    for (const char* p = val; *p; p++) runningSum_ += (uint8_t)*p;
    runningSum_ += '\r';
    runningSum_ += '\n';

    if (strcmp(key, "Checksum") == 0) {
        pending_.checksumOk = (runningSum_ == 0);
        frame_ = pending_;
        pending_ = {};
        runningSum_ = 0;
        return true;
    }

    char* end;
    long raw = strtol(val, &end, 10);
    if (end == val) return false;  // not numeric — ignore

    if (strcmp(key, "V") == 0) {
        pending_.voltage = raw * 0.001f;
        pending_.hasData = true;
    } else if (strcmp(key, "I") == 0) {
        pending_.current = raw * 0.001f;
        pending_.hasData = true;
    } else if (strcmp(key, "SOC") == 0) {
        pending_.soc = raw * 0.1f;
        pending_.hasData = true;
    } else if (strcmp(key, "P") == 0) {
        pending_.power = (float)raw;
        pending_.hasData = true;
    } else if (strcmp(key, "TTG") == 0) {
        pending_.ttg = (raw < 0) ? -1.0f : (float)raw;
        pending_.hasData = true;
    } else if (strcmp(key, "CE") == 0) {
        pending_.consumedAh = raw * -0.001f;  // CE is negative in VE.Direct
        pending_.hasData = true;
    }

    return false;
}
