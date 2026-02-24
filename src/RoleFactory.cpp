#include "RoleFactory.h"

#include <cstring>

#include "AisSimulatorRole.h"
#include "FluidLevelSensorRole.h"
#include "WifiGateway0183Role.h"
#include "WifiGatewayRole.h"

// On ESP32, provide a default TcpServerCreator that builds real TcpServer
// instances. In native tests, callers pass their own creator to inject fakes.
#ifdef ESP32
static TcpServerCreator defaultTcpCreator() {
    return []() { return std::make_unique<TcpServer>(); };
}
#else
static TcpServerCreator defaultTcpCreator() { return nullptr; }
#endif

RoleFactory::RoleFactory(CurrentSensorManagerInterface& currentSensorManager,
                         Nmea2000ServiceInterface& nmea,
                         WifiServiceInterface& wifi,
                         PlatformInterface& platform,
                         TcpServerCreator tcpCreator)
    : currentSensorManager_(currentSensorManager),
      nmea_(nmea),
      wifi_(wifi),
      platform_(platform),
      tcpCreator_(tcpCreator ? std::move(tcpCreator) : defaultTcpCreator()) {}

std::unique_ptr<Role> RoleFactory::createRole(const char* type,
                                              const JsonDocument& doc) {
    auto role = createRoleInstance(type);
    if (!role) {
        return nullptr;
    }

    role->configureFromJson(doc);
    return role;
}

std::unique_ptr<Role> RoleFactory::createRoleInstance(const char* type) {
    if (strcmp(type, "FluidLevel") == 0) {
        return std::make_unique<FluidLevelSensorRole>(currentSensorManager_, nmea_);
    }
    if (strcmp(type, "WifiGateway") == 0) {
        return std::make_unique<WifiGatewayRole>(nmea_, wifi_, tcpCreator_());
    }
    if (strcmp(type, "AisSimulator") == 0) {
        return std::make_unique<AisSimulatorRole>(nmea_, platform_);
    }
    if (strcmp(type, "WifiGateway0183") == 0) {
        return std::make_unique<WifiGateway0183Role>(nmea_, wifi_,
                                                     tcpCreator_());
    }

    return nullptr;
}
