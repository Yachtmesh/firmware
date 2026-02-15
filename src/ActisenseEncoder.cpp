#include "ActisenseEncoder.h"

#include <cstring>

// Actisense N2K Raw format constants
static constexpr uint8_t DLE = 0x10;
static constexpr uint8_t STX = 0x02;
static constexpr uint8_t ETX = 0x03;
static constexpr uint8_t N2K_MSG_RECEIVED = 0x93;

// Write a byte to the output buffer, escaping DLE bytes.
// Returns number of bytes written (1 or 2), or 0 if out of space.
static size_t writeByte(unsigned char* buf, size_t& pos, size_t bufSize,
                        uint8_t byte, uint8_t& checksum) {
    checksum += byte;
    if (byte == DLE) {
        if (pos + 2 > bufSize) return 0;
        buf[pos++] = DLE;
        buf[pos++] = DLE;
        return 2;
    }
    if (pos + 1 > bufSize) return 0;
    buf[pos++] = byte;
    return 1;
}

size_t encodeActisense(uint32_t pgn, uint8_t priority, uint8_t source,
                       const unsigned char* data, size_t dataLen,
                       unsigned char* outBuf, size_t outBufSize) {
    // Worst case: header(2) + escaped payload + trailer(2)
    // Payload: command(1) + len(1) + priority(1) + pgn(3) + dst(1) + src(1) +
    //          timestamp(4) + data(dataLen) + checksum(1)
    // Each byte could be escaped (doubled), so worst case ~2*(13+dataLen)+4
    if (outBufSize < 4 || dataLen > 223) return 0;

    size_t pos = 0;

    // Start: DLE STX (not escaped, not checksummed)
    outBuf[pos++] = DLE;
    outBuf[pos++] = STX;

    uint8_t checksum = 0;

    // Command byte
    if (!writeByte(outBuf, pos, outBufSize - 2, N2K_MSG_RECEIVED, checksum))
        return 0;

    // Length: priority(1) + pgn(3) + dst(1) + src(1) + timestamp(4) + data
    uint8_t len = static_cast<uint8_t>(11 + dataLen);
    if (!writeByte(outBuf, pos, outBufSize - 2, len, checksum)) return 0;

    // Priority
    if (!writeByte(outBuf, pos, outBufSize - 2, priority, checksum)) return 0;

    // PGN (3 bytes, little-endian)
    if (!writeByte(outBuf, pos, outBufSize - 2, pgn & 0xFF, checksum)) return 0;
    if (!writeByte(outBuf, pos, outBufSize - 2, (pgn >> 8) & 0xFF, checksum))
        return 0;
    if (!writeByte(outBuf, pos, outBufSize - 2, (pgn >> 16) & 0xFF, checksum))
        return 0;

    // Destination (broadcast = 0xFF)
    if (!writeByte(outBuf, pos, outBufSize - 2, 0xFF, checksum)) return 0;

    // Source
    if (!writeByte(outBuf, pos, outBufSize - 2, source, checksum)) return 0;

    // Timestamp (4 bytes, set to 0)
    for (int i = 0; i < 4; i++) {
        if (!writeByte(outBuf, pos, outBufSize - 2, 0x00, checksum)) return 0;
    }

    // Data payload
    for (size_t i = 0; i < dataLen; i++) {
        if (!writeByte(outBuf, pos, outBufSize - 2, data[i], checksum))
            return 0;
    }

    // Checksum (two's complement of sum, so total including checksum = 0)
    uint8_t chk = static_cast<uint8_t>(256 - checksum);
    if (!writeByte(outBuf, pos, outBufSize - 2, chk, checksum)) return 0;

    // End: DLE ETX
    if (pos + 2 > outBufSize) return 0;
    outBuf[pos++] = DLE;
    outBuf[pos++] = ETX;

    return pos;
}
