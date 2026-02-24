#pragma once
#include <ArduinoJson.h>

#include <functional>
#include <memory>

#include "CurrentSensorManager.h"
#include "NMEA2000Service.h"
#include "Platform.h"
#include "Role.h"
#include "TcpServer.h"
#include "WifiService.h"

using TcpServerCreator = std::function<std::unique_ptr<TcpServerInterface>()>;

class RoleFactory {
   public:
    RoleFactory(CurrentSensorManagerInterface& currentSensorManager, Nmea2000ServiceInterface& nmea,
                WifiServiceInterface& wifi, PlatformInterface& platform,
                TcpServerCreator tcpCreator = nullptr);

    std::unique_ptr<Role> createRole(const char* type, const JsonDocument& doc);

   private:
    std::unique_ptr<Role> createRoleInstance(const char* type);

    CurrentSensorManagerInterface& currentSensorManager_;
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
    PlatformInterface& platform_;
    TcpServerCreator tcpCreator_;
};
