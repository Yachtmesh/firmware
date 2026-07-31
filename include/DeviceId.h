#pragma once

#include <cstdint>
#include <string>

// Derive a 6-character, base-36 (A-Z0-9) device identifier from a 6-byte MAC
// address. Deterministic: the same MAC always yields the same ID.
std::string deviceIdFromMac(const uint8_t mac[6]);
