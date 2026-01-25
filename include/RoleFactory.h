#pragma once
#include <memory>

#include "NMEA2000Service.h"
#include "Role.h"
#include "all.h"

// Factory that creates Role instances given a type string
// Dependencies injected via constructor, limited to AnalogInput and
// Nmea2000Service for now.
class RoleFactory {
   public:
    RoleFactory(AnalogInputInterface& analog, Nmea2000ServiceInterface& nmea);

    // Creates a Role based on type string, returns nullptr if unknown
    std::unique_ptr<Role> createRole(const char* type);

   private:
    AnalogInputInterface& analog_;
    Nmea2000ServiceInterface& nmea_;
};
