#include "AisEncoder.h"

#include <cctype>
#include <cstring>

// Little-endian helpers matching N2K wire format
static void putU8(uint8_t* buf, int& idx, uint8_t v) {
    buf[idx++] = v;
}

static void putU16LE(uint8_t* buf, int& idx, uint16_t v) {
    buf[idx++] = v & 0xFF;
    buf[idx++] = (v >> 8) & 0xFF;
}

static void putI32LE(uint8_t* buf, int& idx, int32_t v) {
    buf[idx++] = v & 0xFF;
    buf[idx++] = (v >> 8) & 0xFF;
    buf[idx++] = (v >> 16) & 0xFF;
    buf[idx++] = (v >> 24) & 0xFF;
}

static void putU32LE(uint8_t* buf, int& idx, uint32_t v) {
    buf[idx++] = v & 0xFF;
    buf[idx++] = (v >> 8) & 0xFF;
    buf[idx++] = (v >> 16) & 0xFF;
    buf[idx++] = (v >> 24) & 0xFF;
}

static int32_t encode4ByteDouble(double v, double precision) {
    return static_cast<int32_t>(round(v / precision));
}

static uint16_t encode2ByteUDouble(double v, double precision) {
    double vd = round(v / precision);
    if (vd < 0 || vd > 65533) return 0xFFFF;
    return static_cast<uint16_t>(vd);
}

size_t buildPgn129039(const AisClassBPosition& pos, uint8_t* buf, size_t bufSize) {
    if (bufSize < PGN_AIS_CLASS_B_SIZE) return 0;

    int idx = 0;

    // Byte 0: repeat[7:6] | messageID[5:0]
    putU8(buf, idx, 18 & 0x3F);  // messageID=18, repeat=0

    // Bytes 1-4: UserID (MMSI)
    putU32LE(buf, idx, pos.mmsi);

    // Bytes 5-8: Longitude (degrees, 1e-7 precision — NMEA2000 lib convention)
    putI32LE(buf, idx, encode4ByteDouble(pos.longitude, 1e-07));

    // Bytes 9-12: Latitude (degrees, 1e-7 precision — NMEA2000 lib convention)
    putI32LE(buf, idx, encode4ByteDouble(pos.latitude, 1e-07));

    // Byte 13: Seconds[7:2] | RAIM[1] | Accuracy[0]
    putU8(buf, idx, (pos.seconds & 0x3F) << 2);

    // Bytes 14-15: COG (1e-4 rad, unsigned)
    double cogRad = pos.cog * M_PI / 180.0;
    putU16LE(buf, idx, encode2ByteUDouble(cogRad, 1e-04));

    // Bytes 16-17: SOG (0.01 m/s, unsigned)
    double sogMs = pos.sog * 1852.0 / 3600.0;
    putU16LE(buf, idx, encode2ByteUDouble(sogMs, 0.01));

    // Bytes 18-20: Communication State (19 bits) + AIS Transceiver Info (5 bits)
    putU8(buf, idx, 0xFF);
    putU8(buf, idx, 0xFF);
    putU8(buf, idx, (0x00 << 3) | 0x07);  // Channel A VDL reception

    // Bytes 21-22: Heading (1e-4 rad, unsigned)
    double hdgRad = pos.heading * M_PI / 180.0;
    putU16LE(buf, idx, encode2ByteUDouble(hdgRad, 1e-04));

    // Byte 23: Regional application (reserved)
    putU8(buf, idx, 0xFF);

    // Byte 24: Mode[7] | Msg22[6] | Band[5] | DSC[4] | Display[3] | Unit[2] | reserved[1:0]
    // Class B CS defaults: Unit=1(CS), Display=0, DSC=0, Band=1, Msg22=1, Mode=0(autonomous)
    putU8(buf, idx, (0 << 7) | (1 << 6) | (1 << 5) | (0 << 4) | (0 << 3) | (1 << 2));

    // Byte 25: reserved[7:1] | State[0]
    putU8(buf, idx, 0xFE);  // State=0 (not assigned)

    // Byte 26: SID
    putU8(buf, idx, 0xFF);  // N/A

    return static_cast<size_t>(idx);
}

// AIS string: uppercase, 0x20-0x5F range, padded with '@' (0x40)
static void putAisStr(uint8_t* buf, int& idx, const char* str, int maxLen) {
    int i = 0;
    if (str) {
        for (; i < maxLen && str[i] != '\0'; i++) {
            char c = static_cast<char>(toupper(static_cast<unsigned char>(str[i])));
            buf[idx++] = (c >= 0x20 && c <= 0x5F) ? c : '?';
        }
    }
    for (; i < maxLen; i++) {
        buf[idx++] = '@';
    }
}

size_t buildPgn129809(const AisClassBStaticA& data, uint8_t* buf, size_t bufSize) {
    if (bufSize < PGN_AIS_CLASS_B_STATIC_A_SIZE) return 0;

    int idx = 0;

    // Byte 0: repeat[7:6] | messageID[5:0]
    putU8(buf, idx, 24 & 0x3F);  // messageID=24 (Type 24), repeat=0

    // Bytes 1-4: UserID (MMSI)
    putU32LE(buf, idx, data.mmsi);

    // Bytes 5-24: Name (20 bytes, AIS string encoding)
    putAisStr(buf, idx, data.name, 20);

    // Byte 25: Reserved[7:5] | AIS Transceiver Info[4:0]
    putU8(buf, idx, 0xE0);  // reserved=0x7, channel A VDL reception (0)

    // Byte 26: SID
    putU8(buf, idx, 0xFF);

    return static_cast<size_t>(idx);
}
