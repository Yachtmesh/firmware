#include "RoleFactory.h"

#include <cstring>

#include "AisSimulatorRole.h"
#include "FluidLevelSensorRole.h"
#include "VeDirectBatteryRole.h"
#include "WeatherStationRole.h"
#include "UdpBroadcaster.h"
#include "WifiGateway0183Role.h"
#include "WifiGatewayRole.h"

// On ESP32, provide default creators for TCP and UDP transports.
// In native tests, callers pass their own creator (tcpCreator) to inject fakes.
#ifdef ESP32
static TcpServerCreator defaultTcpCreator() {
    return []() { return std::make_unique<TcpServer>(); };
}
static TcpServerCreator defaultUdpCreator() {
    return []() { return std::make_unique<UdpBroadcaster>(); };
}
#else
static TcpServerCreator defaultTcpCreator() { return nullptr; }
static TcpServerCreator defaultUdpCreator() { return nullptr; }
#endif

RoleFactory::RoleFactory(AnalogInputManagerInterface& analogInputManager,
                         Nmea2000ServiceInterface& nmea,
                         WifiServiceInterface& wifi,
                         PlatformInterface& platform,
                         EnvironmentalSensorInterface& envSensor,
                         SerialSensorInterface& serialSensor,
                         TcpServerCreator tcpCreator)
    : analogInputManager_(analogInputManager),
      nmea_(nmea),
      wifi_(wifi),
      platform_(platform),
      envSensor_(envSensor),
      serialSensor_(serialSensor),
      tcpCreator_(tcpCreator ? std::move(tcpCreator) : defaultTcpCreator()),
    udpCreator_(defaultUdpCreator()) {}

std::unique_ptr<Role> RoleFactory::createRole(const char* type,
                                              const JsonDocument& doc) {
    std::unique_ptr<Role> role;

    // WiFi gateway transport is chosen here (where JSON is available) rather
    // than inside the role, so ESP32-specific socket classes never appear in
    // native test builds.
    if (strcmp(type, "WifiGateway") == 0) {
        const char* protocol = doc["protocol"] | "tcp";
        bool wantUdp = (strncmp(protocol, "udp", 3) == 0);
        auto& creator = (wantUdp && udpCreator_) ? udpCreator_ : tcpCreator_;
        role = std::make_unique<WifiGatewayRole>(nmea_, wifi_, creator());
    } else if (strcmp(type, "WifiGateway0183") == 0) {
        const char* protocol = doc["protocol"] | "tcp";
        bool wantUdp = (strncmp(protocol, "udp", 3) == 0);
        auto& creator = (wantUdp && udpCreator_) ? udpCreator_ : tcpCreator_;
        role = std::make_unique<WifiGateway0183Role>(nmea_, wifi_, creator());
    } else {
        role = createRoleInstance(type);
    }

    if (!role) return nullptr;
    role->configureFromJson(doc);
    return role;
}

std::unique_ptr<Role> RoleFactory::createRoleInstance(const char* type) {
    if (strcmp(type, "FluidLevel") == 0) {
        return std::make_unique<FluidLevelSensorRole>(analogInputManager_, nmea_);
    }
    // WifiGateway and WifiGateway0183 are handled in createRole (need JSON for protocol selection).
    if (strcmp(type, "AisSimulator") == 0) {
        return std::make_unique<AisSimulatorRole>(nmea_, platform_);
    }
    if (strcmp(type, "WeatherStation") == 0) {
        return std::make_unique<WeatherStationRole>(envSensor_, nmea_, platform_);
    }
    if (strcmp(type, "VeDirectBattery") == 0) {
        return std::make_unique<VeDirectBatteryRole>(nmea_, serialSensor_);
    }

    return nullptr;
}
