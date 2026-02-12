#pragma once
#include <ArduinoJson.h>

#include <memory>

#include "AnalogInputService.h"
#include "NMEA2000Service.h"
#include "Role.h"
#include "WifiService.h"

// Factory that creates Role instances given a type string
// Dependencies injected via constructor.
class RoleFactory {
   public:
    RoleFactory(AnalogInputInterface& analog, Nmea2000ServiceInterface& nmea,
                WifiServiceInterface& wifi);

    // Creates a configured Role from type string and JSON config
    // Returns nullptr if type is unknown or configuration fails
    std::unique_ptr<Role> createRole(const char* type, const JsonDocument& doc);

   private:
    // Creates an unconfigured Role based on type string
    std::unique_ptr<Role> createRoleInstance(const char* type);

    AnalogInputInterface& analog_;
    Nmea2000ServiceInterface& nmea_;
    WifiServiceInterface& wifi_;
};
