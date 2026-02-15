#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Converts raw N2K AIS messages to NMEA 0183 !AIVDM sentences.
// No ESP32 dependencies — fully native-testable.
class AisN2kTo0183Converter {
   public:
    // Returns !AIVDM sentence (with \r\n) or empty string if PGN not supported
    std::string convert(uint32_t pgn, const unsigned char* data, size_t len);

   private:
    std::string convertPgn129039(const unsigned char* data, size_t len);
    std::string convertPgn129809(const unsigned char* data, size_t len);

    // AIS 6-bit encoding helpers
    void setBits(uint8_t* bits, int& bitIdx, uint32_t value, int numBits);
    std::string ais6BitEncode(const uint8_t* bits, int totalBits);
    std::string formatVdm(const std::string& payload, int fillBits = 0);
};
