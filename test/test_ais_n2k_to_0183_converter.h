#pragma once

#include <unity.h>

#include <cstring>
#include <string>

#include "AisEncoder.h"
#include "AisN2kTo0183Converter.h"

// Helper: verify NMEA 0183 checksum
static bool verifyNmeaChecksum(const std::string& sentence) {
    size_t bangPos = sentence.find('!');
    size_t starPos = sentence.find('*');
    if (bangPos == std::string::npos || starPos == std::string::npos)
        return false;
    uint8_t checksum = 0;
    for (size_t i = bangPos + 1; i < starPos; i++) {
        checksum ^= static_cast<uint8_t>(sentence[i]);
    }
    char expected[3];
    snprintf(expected, sizeof(expected), "%02X", checksum);
    return sentence.substr(starPos + 1, 2) == expected;
}

// Helper: decode AIS 6-bit payload back to bits for field extraction
static void decodeAisPayload(const std::string& payload, uint8_t* bits,
                             int maxBits) {
    memset(bits, 0, (maxBits + 7) / 8);
    int bitIdx = 0;
    for (char c : payload) {
        uint8_t val;
        if (c >= 48 && c <= 87) {
            val = c - 48;
        } else if (c >= 96 && c <= 119) {
            val = c - 56;
        } else {
            val = 0;
        }
        for (int b = 5; b >= 0 && bitIdx < maxBits; b--, bitIdx++) {
            int byteIdx = bitIdx / 8;
            int bitPos = 7 - (bitIdx % 8);
            if (val & (1 << b)) {
                bits[byteIdx] |= (1 << bitPos);
            }
        }
    }
}

// Helper: extract unsigned value from bit array
static uint32_t extractBitsU(const uint8_t* bits, int startBit, int numBits) {
    uint32_t val = 0;
    for (int i = 0; i < numBits; i++) {
        int bitIdx = startBit + i;
        int byteIdx = bitIdx / 8;
        int bitPos = 7 - (bitIdx % 8);
        if (bits[byteIdx] & (1 << bitPos)) {
            val |= (1u << (numBits - 1 - i));
        }
    }
    return val;
}

// Helper: extract signed value from bit array
static int32_t extractBitsS(const uint8_t* bits, int startBit, int numBits) {
    uint32_t raw = extractBitsU(bits, startBit, numBits);
    // Sign extend
    if (raw & (1u << (numBits - 1))) {
        raw |= ~((1u << numBits) - 1);
    }
    return static_cast<int32_t>(raw);
}

// --- PGN 129039 (AIS Class B Position) tests ---

void test_converter_pgn129039_produces_aivdm() {
    AisN2kTo0183Converter converter;

    AisClassBPosition pos = {
        .mmsi = 211234567,
        .latitude = 52.0,
        .longitude = 4.3,
        .sog = 5.0,
        .cog = 180.0,
        .heading = 175.0,
        .seconds = 30};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    std::string result = converter.convert(129039, n2kBuf, PGN_AIS_CLASS_B_SIZE);
    TEST_ASSERT_TRUE(result.size() > 0);
    TEST_ASSERT_TRUE(result.find("!AIVDM") == 0);
    TEST_ASSERT_TRUE(result.find("\r\n") == result.size() - 2);
}

void test_converter_pgn129039_checksum_valid() {
    AisN2kTo0183Converter converter;

    AisClassBPosition pos = {
        .mmsi = 211234567,
        .latitude = 52.0,
        .longitude = 4.3,
        .sog = 5.0,
        .cog = 180.0,
        .heading = 175.0,
        .seconds = 30};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    std::string result = converter.convert(129039, n2kBuf, PGN_AIS_CLASS_B_SIZE);
    TEST_ASSERT_TRUE(verifyNmeaChecksum(result));
}

void test_converter_pgn129039_mmsi() {
    AisN2kTo0183Converter converter;

    AisClassBPosition pos = {
        .mmsi = 211234567,
        .latitude = 52.0,
        .longitude = 4.3,
        .sog = 5.0,
        .cog = 180.0,
        .heading = 175.0,
        .seconds = 30};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    std::string result = converter.convert(129039, n2kBuf, PGN_AIS_CLASS_B_SIZE);

    // Extract payload between 5th comma and 6th comma
    int commaCount = 0;
    size_t payloadStart = 0, payloadEnd = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i] == ',') {
            commaCount++;
            if (commaCount == 5) payloadStart = i + 1;
            if (commaCount == 6) {
                payloadEnd = i;
                break;
            }
        }
    }
    std::string payload = result.substr(payloadStart, payloadEnd - payloadStart);

    uint8_t bits[21];
    decodeAisPayload(payload, bits, 168);

    // Message type: bits 0-5
    uint32_t msgType = extractBitsU(bits, 0, 6);
    TEST_ASSERT_EQUAL(18, msgType);

    // MMSI: bits 8-37
    uint32_t mmsi = extractBitsU(bits, 8, 30);
    TEST_ASSERT_EQUAL(211234567, mmsi);
}

void test_converter_pgn129039_position() {
    AisN2kTo0183Converter converter;

    AisClassBPosition pos = {
        .mmsi = 123456789,
        .latitude = 52.37,
        .longitude = 4.9,
        .sog = 0.0,
        .cog = 0.0,
        .heading = 0.0,
        .seconds = 0};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    std::string result = converter.convert(129039, n2kBuf, PGN_AIS_CLASS_B_SIZE);

    // Extract and decode payload
    int commaCount = 0;
    size_t payloadStart = 0, payloadEnd = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i] == ',') {
            commaCount++;
            if (commaCount == 5) payloadStart = i + 1;
            if (commaCount == 6) {
                payloadEnd = i;
                break;
            }
        }
    }
    std::string payload = result.substr(payloadStart, payloadEnd - payloadStart);

    uint8_t bits[21];
    decodeAisPayload(payload, bits, 168);

    // Longitude: bits 57-84 (28 bits, signed), in 1/10000 minute
    int32_t aisLon = extractBitsS(bits, 57, 28);
    // 4.9 degrees = 294 minutes → 2940000 in 1/10000 min
    double lonDeg = aisLon / 600000.0;
    TEST_ASSERT_FLOAT_WITHIN(0.001, 4.9, lonDeg);

    // Latitude: bits 85-111 (27 bits, signed), in 1/10000 minute
    int32_t aisLat = extractBitsS(bits, 85, 27);
    double latDeg = aisLat / 600000.0;
    TEST_ASSERT_FLOAT_WITHIN(0.001, 52.37, latDeg);
}

void test_converter_pgn129039_sog_cog() {
    AisN2kTo0183Converter converter;

    AisClassBPosition pos = {
        .mmsi = 123456789,
        .latitude = 52.0,
        .longitude = 4.0,
        .sog = 7.5,
        .cog = 135.0,
        .heading = 130.0,
        .seconds = 15};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_SIZE];
    buildPgn129039(pos, n2kBuf, sizeof(n2kBuf));

    std::string result = converter.convert(129039, n2kBuf, PGN_AIS_CLASS_B_SIZE);

    int commaCount = 0;
    size_t payloadStart = 0, payloadEnd = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i] == ',') {
            commaCount++;
            if (commaCount == 5) payloadStart = i + 1;
            if (commaCount == 6) {
                payloadEnd = i;
                break;
            }
        }
    }
    std::string payload = result.substr(payloadStart, payloadEnd - payloadStart);

    uint8_t bits[21];
    decodeAisPayload(payload, bits, 168);

    // SOG: bits 46-55 (10 bits), in 1/10 knot
    uint32_t aisSog = extractBitsU(bits, 46, 10);
    double sogKnots = aisSog / 10.0;
    TEST_ASSERT_FLOAT_WITHIN(0.2, 7.5, sogKnots);

    // COG: bits 112-123 (12 bits), in 1/10 degree
    uint32_t aisCog = extractBitsU(bits, 112, 12);
    double cogDeg = aisCog / 10.0;
    TEST_ASSERT_FLOAT_WITHIN(0.2, 135.0, cogDeg);

    // Heading: bits 124-132 (9 bits), in degrees
    uint32_t aisHdg = extractBitsU(bits, 124, 9);
    TEST_ASSERT_INT_WITHIN(1, 130, aisHdg);
}

void test_converter_pgn129039_short_data_returns_empty() {
    AisN2kTo0183Converter converter;
    unsigned char data[10] = {0};
    std::string result = converter.convert(129039, data, 10);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// --- PGN 129809 (AIS Class B Static Data Part A) tests ---

void test_converter_pgn129809_produces_aivdm() {
    AisN2kTo0183Converter converter;

    AisClassBStaticA staticData = {.mmsi = 211234567, .name = "TESTVESSEL"};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_STATIC_A_SIZE];
    buildPgn129809(staticData, n2kBuf, sizeof(n2kBuf));

    std::string result =
        converter.convert(129809, n2kBuf, PGN_AIS_CLASS_B_STATIC_A_SIZE);
    TEST_ASSERT_TRUE(result.size() > 0);
    TEST_ASSERT_TRUE(result.find("!AIVDM") == 0);
}

void test_converter_pgn129809_checksum_valid() {
    AisN2kTo0183Converter converter;

    AisClassBStaticA staticData = {.mmsi = 211234567, .name = "TESTVESSEL"};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_STATIC_A_SIZE];
    buildPgn129809(staticData, n2kBuf, sizeof(n2kBuf));

    std::string result =
        converter.convert(129809, n2kBuf, PGN_AIS_CLASS_B_STATIC_A_SIZE);
    TEST_ASSERT_TRUE(verifyNmeaChecksum(result));
}

void test_converter_pgn129809_mmsi_and_name() {
    AisN2kTo0183Converter converter;

    AisClassBStaticA staticData = {.mmsi = 211234567, .name = "SEAVOYAGER"};

    uint8_t n2kBuf[PGN_AIS_CLASS_B_STATIC_A_SIZE];
    buildPgn129809(staticData, n2kBuf, sizeof(n2kBuf));

    std::string result =
        converter.convert(129809, n2kBuf, PGN_AIS_CLASS_B_STATIC_A_SIZE);

    // Extract payload
    int commaCount = 0;
    size_t payloadStart = 0, payloadEnd = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i] == ',') {
            commaCount++;
            if (commaCount == 5) payloadStart = i + 1;
            if (commaCount == 6) {
                payloadEnd = i;
                break;
            }
        }
    }
    std::string payload = result.substr(payloadStart, payloadEnd - payloadStart);

    uint8_t bits[21];
    decodeAisPayload(payload, bits, 168);

    // Message type: bits 0-5
    uint32_t msgType = extractBitsU(bits, 0, 6);
    TEST_ASSERT_EQUAL(24, msgType);

    // MMSI: bits 8-37
    uint32_t mmsi = extractBitsU(bits, 8, 30);
    TEST_ASSERT_EQUAL(211234567, mmsi);

    // Part number: bits 38-39
    uint32_t partNum = extractBitsU(bits, 38, 2);
    TEST_ASSERT_EQUAL(0, partNum);

    // Name: bits 40-159 (20 chars × 6 bits)
    char name[21];
    for (int i = 0; i < 20; i++) {
        uint8_t val = static_cast<uint8_t>(extractBitsU(bits, 40 + i * 6, 6));
        if (val == 0) {
            name[i] = '@';
        } else if (val < 32) {
            name[i] = static_cast<char>(val + 0x40);
        } else {
            name[i] = static_cast<char>(val);
        }
    }
    name[20] = '\0';

    // Should start with "SEAVOYAGER" then '@' padding
    TEST_ASSERT_EQUAL_STRING("SEAVOYAGER@@@@@@@@@@", name);
}

void test_converter_pgn129809_short_data_returns_empty() {
    AisN2kTo0183Converter converter;
    unsigned char data[10] = {0};
    std::string result = converter.convert(129809, data, 10);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// --- Unsupported PGN ---

void test_converter_unsupported_pgn_returns_empty() {
    AisN2kTo0183Converter converter;
    unsigned char data[27] = {0};
    std::string result = converter.convert(127505, data, 27);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}
