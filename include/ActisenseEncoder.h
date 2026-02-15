#pragma once

#include <cstddef>
#include <cstdint>

// Encode raw N2K message fields into Actisense binary format.
// Returns number of bytes written to outBuf, or 0 on error.
size_t encodeActisense(uint32_t pgn, uint8_t priority, uint8_t source,
                       const unsigned char* data, size_t dataLen,
                       unsigned char* outBuf, size_t outBufSize);
