#include "WifiGatewayRole.h"

#include <cstdio>
#include <cstring>

// Constructor (shared by native and ESP32)
WifiGatewayRole::WifiGatewayRole(Nmea2000ServiceInterface& nmea,
                                 WifiServiceInterface& wifi)
    : nmea_(nmea), wifi_(wifi) {
#ifdef ESP32
    serverSocket_ = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientSockets_[i] = -1;
    }
#endif
}

void WifiGatewayConfig::toJson(JsonDocument& doc) const {
    doc["type"] = "WifiGateway";
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["port"] = port;
}

const char* WifiGatewayRole::type() {
    return "WifiGateway";
}

void WifiGatewayRole::configure(const RoleConfig& cfg) {
    const auto& c = static_cast<const WifiGatewayConfig&>(cfg);
    config = c;
}

bool WifiGatewayRole::configureFromJson(const JsonDocument& doc) {
    const char* ssid = doc["ssid"] | "";
    const char* password = doc["password"] | "";
    uint16_t port = doc["port"] | 10110;

    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    strncpy(config.password, password, sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    config.port = port;

    return validate();
}

bool WifiGatewayRole::validate() {
    return config.ssid[0] != '\0';
}

void WifiGatewayRole::getConfigJson(JsonDocument& doc) {
    config.toJson(doc);
}

// Non-ESP32 stubs for start/stop/loop/onN2kMessage (native tests)
#ifndef ESP32
void WifiGatewayRole::start() {
    wifi_.connect(config.ssid, config.password);
    nmea_.addListener(this);
    status_.running = true;
}

void WifiGatewayRole::stop() {
    nmea_.removeListener(this);
    wifi_.disconnect();
    status_.running = false;
}

void WifiGatewayRole::loop() {}

void WifiGatewayRole::onN2kMessage(unsigned long, unsigned char,
                                    unsigned char, int,
                                    const unsigned char*, unsigned long) {}
#endif

// Seasmart $PCDIN encoding
// Format: $PCDIN,<PGN 6hex>,<timestamp 8hex>,<source 2hex>,<data hex>*<checksum 2hex>\r\n
size_t encodeSeasmart(unsigned long pgn, unsigned char source, int dataLen,
                      const unsigned char* data, unsigned long timestamp,
                      char* buffer, size_t bufSize) {
    // Minimum needed: "$PCDIN," + 6 + "," + 8 + "," + 2 + "," + dataLen*2 + "*" + 2 + "\r\n" + null
    size_t needed = 7 + 6 + 1 + 8 + 1 + 2 + 1 + (dataLen * 2) + 1 + 2 + 2 + 1;
    if (bufSize < needed) {
        return 0;
    }

    // Build sentence body (after '$', before '*')
    int pos = snprintf(buffer, bufSize, "$PCDIN,%06lX,%08lX,%02X,",
                       pgn, timestamp, source);

    for (int i = 0; i < dataLen; i++) {
        pos += snprintf(buffer + pos, bufSize - pos, "%02X", data[i]);
    }

    // Compute XOR checksum over everything between '$' and '*' (exclusive)
    unsigned char checksum = 0;
    for (int i = 1; i < pos; i++) {
        checksum ^= static_cast<unsigned char>(buffer[i]);
    }

    pos += snprintf(buffer + pos, bufSize - pos, "*%02X\r\n", checksum);

    return pos;
}
