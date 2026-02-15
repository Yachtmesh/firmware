#include "WifiGateway0183Role.h"

#include <cstring>

WifiGateway0183Role::WifiGateway0183Role(Nmea2000ServiceInterface& nmea,
                                         WifiServiceInterface& wifi,
                                         TcpServerInterface& tcpServer)
    : nmea_(nmea), wifi_(wifi), tcpServer_(tcpServer) {}

void WifiGateway0183Config::toJson(JsonDocument& doc) const {
    doc["type"] = "WifiGateway0183";
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["port"] = port;
}

const char* WifiGateway0183Role::type() {
    return "WifiGateway0183";
}

void WifiGateway0183Role::configure(const RoleConfig& cfg) {
    const auto& c = static_cast<const WifiGateway0183Config&>(cfg);
    config = c;
}

bool WifiGateway0183Role::configureFromJson(const JsonDocument& doc) {
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

bool WifiGateway0183Role::validate() {
    return config.ssid[0] != '\0';
}

void WifiGateway0183Role::getConfigJson(JsonDocument& doc) {
    config.toJson(doc);
}

void WifiGateway0183Role::start() {
    wifi_.connect(config.ssid, config.password);
    nmea_.addListener(this);
    status_.running = true;
}

void WifiGateway0183Role::stop() {
    nmea_.removeListener(this);
    tcpServer_.stop();
    tcpStarted_ = false;
    wifi_.disconnect();
    status_.running = false;
}

void WifiGateway0183Role::loop() {
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

void WifiGateway0183Role::onN2kMessage(uint32_t pgn, uint8_t priority,
                                       uint8_t source,
                                       const unsigned char* data, size_t len) {
    std::string sentence = converter_.convert(pgn, data, len);
    if (!sentence.empty()) {
        tcpServer_.sendToAll(sentence.c_str(), sentence.size());
    }
}
