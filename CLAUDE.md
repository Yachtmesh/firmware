# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test Commands

```bash
pio run -e esp32dev              # Build for ESP32
pio run -e esp32dev -t upload    # Upload to device
pio device monitor               # Monitor serial output
pio test -e native               # Run all native tests
```

## Architecture

### Role Pattern

The core abstraction for sensor functionality (`include/Role.h`):

```cpp
class Role {
    virtual const char* type() = 0;
    virtual void configure(const RoleConfig& cfg) = 0;
    virtual bool configureFromJson(const JsonDocument& doc) = 0;
    virtual bool validate() = 0;
    virtual void start() / stop() / loop() = 0;
    virtual RoleStatus status() = 0;
    virtual void getConfigJson(JsonDocument& doc) = 0;
};
```

Each role has an instance ID (e.g., `FluidLevel-axv`) and persists configuration as JSON in `/roles/`.

### Key Classes

- **RoleManager** - Lifecycle orchestration, persistence, BLE config caching
- **RoleFactory** - Creates Role instances from type strings and JSON config
- **FileSystemInterface** - Abstract filesystem for testability (LittleFS in production, MockFileSystem in tests)

### Service Injection

Services are injected into Roles via interfaces:

- `Nmea2000ServiceInterface` - Sends metrics to NMEA2000 bus
- `AnalogInputInterface` - Reads analog sensor values

### Enum Code Generation

`types.h` uses macros to generate type-safe enums with automatic string conversion:

```cpp
#define FLUID_TYPE_LIST(X) X(Fuel) X(Water) X(GrayWater) ...
GENERATE_ENUM(FluidType, FLUID_TYPE_LIST)
GENERATE_TO_STRING(FluidType, FLUID_TYPE_LIST)    // FluidTypeToString()
GENERATE_FROM_STRING(FluidType, FLUID_TYPE_LIST)  // FluidTypeFromString()
```

### Deferred Persistence

RoleManager defers filesystem writes to `loopAll()` to avoid blocking in BLE callbacks:

```cpp
void loopAll() {
    // ... execute roles ...
    if (!pendingPersist_.empty()) {
        persistPendingConfigs();  // Safe I/O context
    }
}
```

## Adding a New Role Type

1. Create `NewSensorRole.h/.cpp` extending `Role`
2. Implement all virtual methods including `configureFromJson()` and `getConfigJson()`
3. Add the type to `RoleFactory::createRoleInstance()`
4. Write tests in `test/test_new_sensor_role.h`
5. Include test header in `test/main.cpp`

## Current Roles

### FluidLevelSensorRole

Reads analog voltage from tank level sensor, broadcasts as NMEA2000 PGN 127505.

Config: `fluidType`, `inst` (instance), `capacity` (liters), `minVoltage`/`maxVoltage` (calibration)

## Dependencies

- NMEA2000-library / NMEA2000_esp32 - NMEA 2000 protocol and ESP32 CAN driver
- ArduinoJson - JSON parsing/serialization
- NimBLE-Arduino - Bluetooth Low Energy
- Unity - Test framework (native only)
