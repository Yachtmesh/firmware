#include "AisN2kTo0183Converter.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Little-endian read helpers matching N2K wire format
static uint8_t getU8(const unsigned char* buf, int idx) {
    return buf[idx];
}

static uint16_t getU16LE(const unsigned char* buf, int idx) {
    return static_cast<uint16_t>(buf[idx]) |
           (static_cast<uint16_t>(buf[idx + 1]) << 8);
}

static int32_t getI32LE(const unsigned char* buf, int idx) {
    return static_cast<int32_t>(buf[idx]) |
           (static_cast<int32_t>(buf[idx + 1]) << 8) |
           (static_cast<int32_t>(buf[idx + 2]) << 16) |
           (static_cast<int32_t>(buf[idx + 3]) << 24);
}

static uint32_t getU32LE(const unsigned char* buf, int idx) {
    return static_cast<uint32_t>(buf[idx]) |
           (static_cast<uint32_t>(buf[idx + 1]) << 8) |
           (static_cast<uint32_t>(buf[idx + 2]) << 16) |
           (static_cast<uint32_t>(buf[idx + 3]) << 24);
}

std::string AisN2kTo0183Converter::convert(uint32_t pgn,
                                           const unsigned char* data,
                                           size_t len) {
    switch (pgn) {
        case 129039:
            return convertPgn129039(data, len);
        case 129809:
            return convertPgn129809(data, len);
        default:
            return "";
    }
}

void AisN2kTo0183Converter::setBits(uint8_t* bits, int& bitIdx,
                                    uint32_t value, int numBits) {
    for (int i = numBits - 1; i >= 0; i--) {
        int byteIdx = bitIdx / 8;
        int bitPos = 7 - (bitIdx % 8);
        if (value & (1u << i)) {
            bits[byteIdx] |= (1 << bitPos);
        }
        bitIdx++;
    }
}

std::string AisN2kTo0183Converter::ais6BitEncode(const uint8_t* bits,
                                                  int totalBits) {
    std::string result;
    int numChars = (totalBits + 5) / 6;
    for (int i = 0; i < numChars; i++) {
        uint8_t val = 0;
        for (int b = 0; b < 6; b++) {
            int bitPos = i * 6 + b;
            if (bitPos < totalBits) {
                int byteIdx = bitPos / 8;
                int bitOff = 7 - (bitPos % 8);
                if (bits[byteIdx] & (1 << bitOff)) {
                    val |= (1 << (5 - b));
                }
            }
        }
        // AIS 6-bit ASCII armor: value 0-39 → char 48-87, 40-63 → char 96-119
        char c;
        if (val < 40) {
            c = static_cast<char>(val + 48);
        } else {
            c = static_cast<char>(val + 56);
        }
        result += c;
    }
    return result;
}

// Compute NMEA 0183 checksum and format complete sentence
std::string AisN2kTo0183Converter::formatVdm(const std::string& payload,
                                             int fillBits) {
    // !AIVDM,1,1,,A,<payload>,<fill>*<checksum>\r\n
    char sentence[256];
    snprintf(sentence, sizeof(sentence), "!AIVDM,1,1,,A,%s,%d",
             payload.c_str(), fillBits);

    // XOR checksum of everything between ! and * (exclusive)
    uint8_t checksum = 0;
    for (const char* p = sentence + 1; *p; p++) {
        checksum ^= static_cast<uint8_t>(*p);
    }

    char result[270];
    snprintf(result, sizeof(result), "%s*%02X\r\n", sentence, checksum);
    return result;
}

// AIS Message 18: Class B CS Position Report (168 bits)
std::string AisN2kTo0183Converter::convertPgn129039(const unsigned char* data,
                                                     size_t len) {
    if (len < 27) return "";

    // Extract fields from N2K payload (matching AisEncoder layout)
    // Byte 0: repeat[7:6] | messageID[5:0]
    // uint8_t messageId = getU8(data, 0) & 0x3F;
    uint32_t mmsi = getU32LE(data, 1);
    int32_t lonRaw = getI32LE(data, 5);   // 1e-7 degrees
    int32_t latRaw = getI32LE(data, 9);   // 1e-7 degrees
    uint8_t secByte = getU8(data, 13);
    uint16_t cogRaw = getU16LE(data, 14);  // 1e-4 rad
    uint16_t sogRaw = getU16LE(data, 16);  // 0.01 m/s
    uint16_t hdgRaw = getU16LE(data, 21);  // 1e-4 rad

    // Convert to AIS units
    // N2K stores degrees * 1e7. AIS stores 1/10000 minute.
    // 1 degree = 60 minutes, so AIS = N2K_raw * 60 * 10000 / 1e7 = N2K_raw * 0.06
    int32_t aisLon = static_cast<int32_t>(round(lonRaw * 0.06));
    int32_t aisLat = static_cast<int32_t>(round(latRaw * 0.06));

    // SOG: N2K is 0.01 m/s, AIS is 1/10 knot
    // 1 knot = 1852/3600 m/s = 0.5144 m/s
    // SOG_knots = sogRaw * 0.01 / 0.5144
    // SOG_AIS = SOG_knots * 10 = sogRaw * 0.01 / 0.5144 * 10
    uint16_t aisSog = (sogRaw == 0xFFFF)
                          ? 1023
                          : static_cast<uint16_t>(
                                round(sogRaw * 0.01 / 0.51444 * 10.0));

    // COG: N2K is 1e-4 rad, AIS is 1/10 degree
    // COG_deg = cogRaw * 1e-4 * 180 / PI
    // COG_AIS = COG_deg * 10
    uint16_t aisCog = (cogRaw == 0xFFFF)
                          ? 3600
                          : static_cast<uint16_t>(
                                round(cogRaw * 1e-4 * 180.0 / M_PI * 10.0));

    // Heading: same conversion as COG
    uint16_t aisHdg = (hdgRaw == 0xFFFF)
                          ? 511
                          : static_cast<uint16_t>(
                                round(hdgRaw * 1e-4 * 180.0 / M_PI));

    // Seconds
    uint8_t seconds = (secByte >> 2) & 0x3F;

    // Build AIS Message 18 bit vector (168 bits)
    uint8_t bits[21];  // 168 bits = 21 bytes
    memset(bits, 0, sizeof(bits));
    int bitIdx = 0;

    setBits(bits, bitIdx, 18, 6);                               // Message Type
    setBits(bits, bitIdx, 0, 2);                                // Repeat
    setBits(bits, bitIdx, mmsi, 30);                            // MMSI
    setBits(bits, bitIdx, 0, 8);                                // Reserved
    setBits(bits, bitIdx, static_cast<uint32_t>(aisSog), 10);   // SOG
    setBits(bits, bitIdx, 0, 1);                                // Accuracy
    setBits(bits, bitIdx, static_cast<uint32_t>(aisLon) & 0x0FFFFFFF, 28);  // Longitude
    setBits(bits, bitIdx, static_cast<uint32_t>(aisLat) & 0x07FFFFFF, 27);  // Latitude
    setBits(bits, bitIdx, static_cast<uint32_t>(aisCog), 12);   // COG
    setBits(bits, bitIdx, static_cast<uint32_t>(aisHdg), 9);    // Heading
    setBits(bits, bitIdx, seconds, 6);                          // Seconds
    setBits(bits, bitIdx, 0, 2);                                // Regional
    setBits(bits, bitIdx, 1, 1);                                // CS Unit (1=CS)
    setBits(bits, bitIdx, 0, 1);                                // Display
    setBits(bits, bitIdx, 0, 1);                                // DSC
    setBits(bits, bitIdx, 1, 1);                                // Band
    setBits(bits, bitIdx, 1, 1);                                // Msg22
    setBits(bits, bitIdx, 0, 1);                                // Assigned
    setBits(bits, bitIdx, 0, 1);                                // RAIM
    setBits(bits, bitIdx, 0, 20);                               // Radio status

    std::string payload = ais6BitEncode(bits, 168);
    return formatVdm(payload, 0);
}

// AIS Message 24 Part A: Class B Static Data (vessel name) (168 bits)
std::string AisN2kTo0183Converter::convertPgn129809(const unsigned char* data,
                                                     size_t len) {
    if (len < 27) return "";

    // Byte 0: repeat[7:6] | messageID[5:0]
    uint32_t mmsi = getU32LE(data, 1);
    // Bytes 5-24: Name (20 bytes, AIS string, uppercase, '@'-padded)
    char name[21];
    memcpy(name, data + 5, 20);
    name[20] = '\0';

    // Build AIS Message 24 Part A bit vector (168 bits)
    uint8_t bits[21];
    memset(bits, 0, sizeof(bits));
    int bitIdx = 0;

    setBits(bits, bitIdx, 24, 6);     // Message Type
    setBits(bits, bitIdx, 0, 2);      // Repeat
    setBits(bits, bitIdx, mmsi, 30);  // MMSI
    setBits(bits, bitIdx, 0, 2);      // Part Number (0 = Part A)

    // Name: 20 characters × 6 bits = 120 bits
    for (int i = 0; i < 20; i++) {
        char c = name[i];
        uint8_t val;
        if (c == '@' || c == '\0') {
            val = 0;  // '@' padding = AIS 6-bit 0
        } else if (c >= 0x40 && c <= 0x5F) {
            val = c - 0x40;  // A-Z and some symbols
        } else if (c >= 0x20 && c <= 0x3F) {
            val = c;  // space, digits, punctuation keep their value
        } else {
            val = 0;  // fallback
        }
        setBits(bits, bitIdx, val, 6);
    }

    // Spare: 8 bits
    setBits(bits, bitIdx, 0, 8);

    std::string payload = ais6BitEncode(bits, 168);
    return formatVdm(payload, 0);
}
