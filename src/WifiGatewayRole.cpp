#include "WifiGatewayRole.h"

#include <cstdio>
#include <cstring>

WifiGatewayRole::WifiGatewayRole(Nmea2000ServiceInterface& nmea,
                                 WifiServiceInterface& wifi,
                                 TcpServerInterface& tcpServer)
    : nmea_(nmea), wifi_(wifi), tcpServer_(tcpServer) {}

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

void WifiGatewayRole::start() {
    wifi_.connect(config.ssid, config.password);
    nmea_.addListener(this);
    status_.running = true;
}

void WifiGatewayRole::stop() {
    nmea_.removeListener(this);
    tcpServer_.stop();
    tcpStarted_ = false;
    wifi_.disconnect();
    status_.running = false;
}

void WifiGatewayRole::loop() {
    if (wifi_.isConnected()) {
        if (!tcpStarted_) {
            tcpStarted_ = tcpServer_.start(config.port);
        }
        tcpServer_.loop();
    } else if (tcpStarted_) {
        tcpServer_.stop();
        tcpStarted_ = false;
    }
}

void WifiGatewayRole::onN2kMessage(unsigned long pgn, unsigned char source,
                                    unsigned char priority, int dataLen,
                                    const unsigned char* data,
                                    unsigned long msgTime) {
    char buffer[256];
    size_t len = encodeSeasmart(pgn, source, dataLen, data, msgTime, buffer,
                                sizeof(buffer));
    if (len > 0) {
        tcpServer_.sendToAll(buffer, len);
    }
}

// Seasmart $PCDIN encoding
// Format: $PCDIN,<PGN 6hex>,<timestamp 8hex>,<source 2hex>,<data hex>*<checksum 2hex>\r\n
size_t encodeSeasmart(unsigned long pgn, unsigned char source, int dataLen,
                      const unsigned char* data, unsigned long timestamp,
                      char* buffer, size_t bufSize) {
    size_t needed = 7 + 6 + 1 + 8 + 1 + 2 + 1 + (dataLen * 2) + 1 + 2 + 2 + 1;
    if (bufSize < needed) {
        return 0;
    }

    int pos = snprintf(buffer, bufSize, "$PCDIN,%06lX,%08lX,%02X,",
                       pgn, timestamp, source);

    for (int i = 0; i < dataLen; i++) {
        pos += snprintf(buffer + pos, bufSize - pos, "%02X", data[i]);
    }

    unsigned char checksum = 0;
    for (int i = 1; i < pos; i++) {
        checksum ^= static_cast<unsigned char>(buffer[i]);
    }

    pos += snprintf(buffer + pos, bufSize - pos, "*%02X\r\n", checksum);

    return pos;
}
