#pragma once
#include <ArduinoJson.h>

#include <functional>
#include <memory>

#include "AnalogInputService.h"
#include "NMEA2000Service.h"
#include "Platform.h"
#include "Role.h"
#include "TcpServer.h"
#include "WifiService.h"

// Factory function type for creating TcpServer instances.
// Roles that need a TCP server get their own instance, allowing
// multiple gateway roles to run on different ports simultaneously.
// Replaceable in tests to inject mock/fake TcpServer implementations.
using TcpServerCreator = std::function<std::unique_ptr<TcpServerInterface>()>;

// Factory that creates Role instances given a type string.
// Shared device resources (NMEA bus, WiFi radio, ADC) are injected as
// references since they're true singletons. Per-role resources like
// TCP servers are created on demand via the TcpServerCreator.
class RoleFactory {
   public:
    RoleFactory(AnalogInputInterface& analog, Nmea2000ServiceInterface& nmea,
                WifiServiceInterface& wifi, PlatformInterface& platform,
                TcpServerCreator tcpCreator = nullptr);

    // Creates a configured Role from type string and JSON config.
    // Returns nullptr if type is unknown or configuration fails.
    std::unique_ptr<Role> createRole(const char* type, const JsonDocument& doc);

   private:
    // Creates an unconfigured Role based on type string.
    std::unique_ptr<Role> createRoleInstance(const char* type);

    AnalogInputInterface& analog_;
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
    PlatformInterface& platform_;
    TcpServerCreator tcpCreator_;
};
