#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// N2K PGN 129039 - AIS Class B Position Report
// Builds the raw data payload matching NMEA 2000 encoding.

struct AisClassBPosition {
    uint32_t mmsi;
    double latitude;   // degrees (+ = N, - = S)
    double longitude;  // degrees (+ = E, - = W)
    double sog;        // knots
    double cog;        // degrees
    double heading;    // degrees
    uint8_t seconds;   // UTC second (0-59, 60 = N/A)
};

static constexpr uint32_t PGN_AIS_CLASS_B_POSITION = 129039;
static constexpr uint8_t PGN_AIS_CLASS_B_PRIORITY = 4;
static constexpr size_t PGN_AIS_CLASS_B_SIZE = 27;

// Build PGN 129039 payload bytes (N2K encoding).
// Returns the number of bytes written to buf, or 0 on error.
size_t buildPgn129039(const AisClassBPosition& pos, uint8_t* buf, size_t bufSize);

// N2K PGN 129809 - AIS Class B Static Data Report, Part A (vessel name)

struct AisClassBStaticA {
    uint32_t mmsi;
    const char* name;  // max 20 chars, will be uppercased and '@'-padded
};

static constexpr uint32_t PGN_AIS_CLASS_B_STATIC_A = 129809;
static constexpr uint8_t PGN_AIS_CLASS_B_STATIC_A_PRIORITY = 6;
static constexpr size_t PGN_AIS_CLASS_B_STATIC_A_SIZE = 27;

// Build PGN 129809 payload bytes (N2K encoding).
size_t buildPgn129809(const AisClassBStaticA& data, uint8_t* buf, size_t bufSize);
