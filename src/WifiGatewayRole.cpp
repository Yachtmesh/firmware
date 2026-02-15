#include "WifiGatewayRole.h"

#include <cstring>

#include "ActisenseEncoder.h"

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

void WifiGatewayRole::onN2kMessage(uint32_t pgn, uint8_t priority,
                                   uint8_t source, const unsigned char* data,
                                   size_t len) {
    unsigned char buf[512];
    size_t encoded =
        encodeActisense(pgn, priority, source, data, len, buf, sizeof(buf));
    if (encoded > 0) {
        tcpServer_.sendToAll(reinterpret_cast<const char*>(buf), encoded);
    }
}
