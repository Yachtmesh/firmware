#pragma once
#include <ArduinoJson.h>

#include <functional>
#include <memory>

#include "AnalogInputManager.h"
#include "EnvironmentalSensorService.h"
#include "NMEA2000Service.h"
#include "Platform.h"
#include "Role.h"
#include "SerialSensorService.h"
#include "TcpServer.h"
#include "WifiService.h"

using TcpServerCreator = std::function<std::unique_ptr<TcpServerInterface>()>;

class RoleFactory {
   public:
    RoleFactory(AnalogInputManagerInterface& analogInputManager,
                Nmea2000ServiceInterface& nmea,
                WifiServiceInterface& wifi,
                PlatformInterface& platform,
                EnvironmentalSensorInterface& envSensor,
                SerialSensorInterface& serialSensor,
                TcpServerCreator tcpCreator = nullptr);

    std::unique_ptr<Role> createRole(const char* type, const JsonDocument& doc);

   private:
    std::unique_ptr<Role> createRoleInstance(const char* type);

    AnalogInputManagerInterface& analogInputManager_;
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
    PlatformInterface& platform_;
    EnvironmentalSensorInterface& envSensor_;
    SerialSensorInterface& serialSensor_;
    TcpServerCreator tcpCreator_;
    TcpServerCreator udpCreator_;
};
