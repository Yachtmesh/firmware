#pragma once

#include <unity.h>

#include <cmath>
#include <cstring>

#include "AisEncoder.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test helper: extract little-endian uint32 from buffer
static uint32_t getU32LE(const uint8_t* buf, int offset) {
    return buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) |
           (buf[offset + 3] << 24);
}

// Test helper: extract little-endian int32 from buffer
static int32_t getI32LE(const uint8_t* buf, int offset) {
    return static_cast<int32_t>(getU32LE(buf, offset));
}

// Test helper: extract little-endian uint16 from buffer
static uint16_t getU16LE(const uint8_t* buf, int offset) {
    return buf[offset] | (buf[offset + 1] << 8);
}

// --- Size and format tests ---

void test_pgn129039_returns_correct_size() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 52.45;
    pos.longitude = 5.05;
    pos.sog = 4.5;
    pos.cog = 45.0;
    pos.heading = 45.0;
    pos.seconds = 60;

    uint8_t buf[32];
    size_t len = buildPgn129039(pos, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_SIZE, len);
}

void test_pgn129039_buffer_too_small() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;

    uint8_t buf[10];
    size_t len = buildPgn129039(pos, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(0, len);
}

void test_pgn129039_message_id() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 52.45;
    pos.longitude = 5.05;
    pos.sog = 4.5;
    pos.cog = 45.0;
    pos.heading = 45.0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // Byte 0: repeat[7:6]=0 | messageID[5:0]=18
    TEST_ASSERT_EQUAL_UINT8(18, buf[0] & 0x3F);
    TEST_ASSERT_EQUAL_UINT8(0, (buf[0] >> 6) & 0x03);
}

void test_pgn129039_mmsi_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 52.45;
    pos.longitude = 5.05;
    pos.sog = 0;
    pos.cog = 0;
    pos.heading = 0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    uint32_t mmsi = getU32LE(buf, 1);
    TEST_ASSERT_EQUAL_UINT32(244000001, mmsi);
}

void test_pgn129039_position_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 52.45;
    pos.longitude = 5.05;
    pos.sog = 4.5;
    pos.cog = 45.0;
    pos.heading = 45.0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // Longitude at bytes 5-8: degrees / 1e-7 (NMEA2000 lib convention)
    int32_t lonRaw = getI32LE(buf, 5);
    int32_t lonExpected = static_cast<int32_t>(round(5.05 / 1e-07));
    TEST_ASSERT_INT32_WITHIN(1, lonExpected, lonRaw);

    // Latitude at bytes 9-12: degrees / 1e-7
    int32_t latRaw = getI32LE(buf, 9);
    int32_t latExpected = static_cast<int32_t>(round(52.45 / 1e-07));
    TEST_ASSERT_INT32_WITHIN(1, latExpected, latRaw);
}

void test_pgn129039_negative_position() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = -33.8;
    pos.longitude = -73.5;
    pos.sog = 0;
    pos.cog = 0;
    pos.heading = 0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    int32_t lonRaw = getI32LE(buf, 5);
    TEST_ASSERT_TRUE(lonRaw < 0);

    int32_t latRaw = getI32LE(buf, 9);
    TEST_ASSERT_TRUE(latRaw < 0);
}

void test_pgn129039_sog_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 0;
    pos.longitude = 0;
    pos.sog = 4.5;  // knots
    pos.cog = 0;
    pos.heading = 0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // SOG at bytes 16-17: knots -> m/s / 0.01
    uint16_t sogRaw = getU16LE(buf, 16);
    double sogMs = 4.5 * 1852.0 / 3600.0;
    uint16_t sogExpected = static_cast<uint16_t>(round(sogMs / 0.01));
    TEST_ASSERT_UINT16_WITHIN(1, sogExpected, sogRaw);
}

void test_pgn129039_cog_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 0;
    pos.longitude = 0;
    pos.sog = 0;
    pos.cog = 123.4;  // degrees
    pos.heading = 0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // COG at bytes 14-15: degrees -> radians / 1e-4
    uint16_t cogRaw = getU16LE(buf, 14);
    double cogRad = 123.4 * M_PI / 180.0;
    uint16_t cogExpected = static_cast<uint16_t>(round(cogRad / 1e-04));
    TEST_ASSERT_UINT16_WITHIN(1, cogExpected, cogRaw);
}

void test_pgn129039_heading_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 0;
    pos.longitude = 0;
    pos.sog = 0;
    pos.cog = 0;
    pos.heading = 45.0;
    pos.seconds = 60;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // Heading at bytes 21-22: degrees -> radians / 1e-4
    uint16_t hdgRaw = getU16LE(buf, 21);
    double hdgRad = 45.0 * M_PI / 180.0;
    uint16_t hdgExpected = static_cast<uint16_t>(round(hdgRad / 1e-04));
    TEST_ASSERT_UINT16_WITHIN(1, hdgExpected, hdgRaw);
}

void test_pgn129039_seconds_encoding() {
    AisClassBPosition pos = {};
    pos.mmsi = 244000001;
    pos.latitude = 0;
    pos.longitude = 0;
    pos.sog = 0;
    pos.cog = 0;
    pos.heading = 0;
    pos.seconds = 30;

    uint8_t buf[32];
    buildPgn129039(pos, buf, sizeof(buf));

    // Byte 13: Seconds[7:2]
    uint8_t seconds = (buf[13] >> 2) & 0x3F;
    TEST_ASSERT_EQUAL_UINT8(30, seconds);
}

// --- PGN 129809 (AIS Class B Static Data Part A) tests ---

void test_pgn129809_returns_correct_size() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = "Bad Bunny";

    uint8_t buf[32];
    size_t len = buildPgn129809(data, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_STATIC_A_SIZE, len);
}

void test_pgn129809_buffer_too_small() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = "Test";

    uint8_t buf[10];
    size_t len = buildPgn129809(data, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(0, len);
}

void test_pgn129809_mmsi_encoding() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = "Test";

    uint8_t buf[32];
    buildPgn129809(data, buf, sizeof(buf));

    uint32_t mmsi = getU32LE(buf, 1);
    TEST_ASSERT_EQUAL_UINT32(244000001, mmsi);
}

void test_pgn129809_message_id() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = "Test";

    uint8_t buf[32];
    buildPgn129809(data, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT8(24, buf[0] & 0x3F);
}

void test_pgn129809_name_uppercase_padded() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = "Bad Bunny";

    uint8_t buf[32];
    buildPgn129809(data, buf, sizeof(buf));

    // Name starts at byte 5, 20 bytes long
    // "Bad Bunny" -> "BAD BUNNY" + 11 '@' padding
    TEST_ASSERT_EQUAL_UINT8('B', buf[5]);
    TEST_ASSERT_EQUAL_UINT8('A', buf[6]);
    TEST_ASSERT_EQUAL_UINT8('D', buf[7]);
    TEST_ASSERT_EQUAL_UINT8(' ', buf[8]);
    TEST_ASSERT_EQUAL_UINT8('B', buf[9]);
    TEST_ASSERT_EQUAL_UINT8('U', buf[10]);
    TEST_ASSERT_EQUAL_UINT8('N', buf[11]);
    TEST_ASSERT_EQUAL_UINT8('N', buf[12]);
    TEST_ASSERT_EQUAL_UINT8('Y', buf[13]);
    TEST_ASSERT_EQUAL_UINT8('@', buf[14]);  // padding starts
    TEST_ASSERT_EQUAL_UINT8('@', buf[24]);  // last padding byte
}

void test_pgn129809_null_name() {
    AisClassBStaticA data = {};
    data.mmsi = 244000001;
    data.name = nullptr;

    uint8_t buf[32];
    size_t len = buildPgn129809(data, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(PGN_AIS_CLASS_B_STATIC_A_SIZE, len);

    // All 20 name bytes should be '@'
    for (int i = 5; i < 25; i++) {
        TEST_ASSERT_EQUAL_UINT8('@', buf[i]);
    }
}
