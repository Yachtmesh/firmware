# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Core Philosophy

**TEST-DRIVEN DEVELOPMENT IS NON-NEGOTIABLE.** Every single line of production code must be written in response to a failing test. No exceptions. This is not a suggestion or a preference - it is the fundamental practice that enables all other principles in this document.

I follow Test-Driven Development (TDD) with a strong emphasis on behavior-driven testing and functional programming principles. All work should be done in small, incremental changes that maintain a working state throughout development.

## Quick Reference

**Key Principles:**

- If asked to write a plan do so in a .md file starting with plan-\* and followed by a 4 word summary of the question
- If asked to write a plan, write it in the file but don't implement anything but wait for further input
- Write tests first (TDD)
- Test behavior, not implementation
- No type assertions
- Immutable data only
- Small, pure functions
- TypeScript strict mode always
- Use real schemas/types in tests, never redefine them

## Testing Principles

**Core principle**: Test behavior, not implementation. 100% coverage through business behavior.

**Quick reference:**

- Write tests first (TDD non-negotiable)
- Test through public API exclusively
- Use factory functions for test data
- Tests must document expected business behavior
- No 1:1 mapping between test files and implementation files

For detailed testing patterns and examples, load the `testing` skill.
For verifying test effectiveness through mutation analysis, load the `mutation-testing` skill.

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

### Services Layer

Services wrap ESP32 platform functionality behind abstract interfaces so Roles remain hardware-independent and native-testable. Each service follows the same pattern:

1. **Abstract interface** — pure virtual class defining the API (e.g., `Nmea2000ServiceInterface`)
2. **ESP32 implementation** — concrete class guarded by `#ifdef ESP32` in the same header (e.g., `Nmea2000Service`)
3. **Wired in `main.cpp`** — instantiated as globals, injected into `RoleFactory` by reference

Current services:

| Interface                   | Implementation       | Purpose                                              |
| --------------------------- | -------------------- | ---------------------------------------------------- |
| `Nmea2000ServiceInterface`  | `Nmea2000Service`    | Send/receive NMEA 2000 messages via CAN/TWAI bus     |
| `AnalogInputInterface`      | `AnalogInputService` | Read analog sensor voltage (ADC)                     |
| `WifiServiceInterface`      | `WifiService`        | Connect/disconnect Wi-Fi STA mode                    |
| `TcpServerInterface`        | `TcpServer`          | Accept TCP connections and broadcast data            |
| `PlatformInterface`         | `Esp32Platform`      | MAC address, device ID persistence, CPU temp, millis |
| `BluetoothServiceInterface` | `BluetoothService`   | BLE GATT server for mobile app config                |

Services are injected into Roles through `RoleFactory`, which holds references to all service interfaces and passes the relevant ones to each Role's constructor.

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

## Adding a New Service

1. **Define the interface** in `include/NewService.h` — a pure virtual class with `virtual ~NewServiceInterface() = default;`
2. **Add the ESP32 implementation** in the same header, guarded by `#ifdef ESP32`, extending the interface
3. **Implement** in `src/NewService.cpp` — add the file to `src/CMakeLists.txt` SRCS list
4. **Wire into `main.cpp`** — instantiate the concrete service and pass it to `RoleFactory`
5. **Add to `RoleFactory`** — accept the new interface reference in the constructor and store it
6. **Create a mock** in `test/MockNewService.h` for native tests (implement the interface with test-controllable behavior)
7. **Never put ESP32 headers in the interface** — keep `#include <esp_*.h>` etc. inside the `#ifdef ESP32` block only so native tests compile cleanly

Key rules:

- Interfaces must be pure virtual (no ESP32 dependencies) so Roles using them remain native-testable
- The `#ifdef ESP32` / concrete implementation pattern keeps everything in one header file per service
- Services that need a `loop()` call should have it invoked from the main loop in `main.cpp`

## Adding a New Role Type

### 1. Define types in `include/types.h` (if the role introduces new enums)

Use the macro generator pattern. Every enum **must** include `Unavailable` as the last value (used as the fallback for unknown strings):

```cpp
#define MY_TYPE_LIST(X) \
    X(ValueA)           \
    X(ValueB)           \
    X(Unavailable)

#define CURRENT_ENUM_NAME MyType
GENERATE_ENUM(MyType, MY_TYPE_LIST)
GENERATE_TO_STRING(MyType, MY_TYPE_LIST)    // MyTypeToString()
GENERATE_FROM_STRING(MyType, MY_TYPE_LIST)  // MyTypeFromString()
#undef CURRENT_ENUM_NAME
```

Key rules for types:

- The `#define CURRENT_ENUM_NAME` / `#undef` pair is required — the macros use it internally
- String conversion is case-sensitive and matches the enum value name exactly
- `FromString()` returns `Unavailable` for any unrecognized input
- Write round-trip tests: `FromString(ToString(value)) == value`

### 2. Create the config struct and role class in `include/NewRole.h`

```cpp
struct NewRoleConfig : public RoleConfig {
    // Config fields with sensible defaults
    void toJson(JsonDocument& doc) const override;
};

class NewRole : public Role {
   public:
    // Constructor takes only service interfaces — never ESP32 types directly
    NewRole(Nmea2000ServiceInterface& nmea);

    // All Role virtuals
    const char* type() override;
    void configure(const RoleConfig& cfg) override;
    bool configureFromJson(const JsonDocument& doc) override;
    bool validate() override;
    void start() override;
    void stop() override;
    void loop() override;
    void getConfigJson(JsonDocument& doc) override;

    NewRoleConfig config;  // public for test access

   private:
    Nmea2000ServiceInterface& nmea_;
};
```

Key rules:

- Roles must **never** include ESP32 headers — all platform access goes through injected service interfaces
- `configureFromJson()` must call `validate()` and return false if validation fails
- `getConfigJson()` must include `"type"` in the output (used for persistence round-trip)
- Store service dependencies as references (`&`), not pointers
- Config struct is public for direct test access

### 3. Implement in `src/NewRole.cpp`

- `type()` returns a short string matching the factory key (e.g., `"FluidLevel"`, `"WifiGateway"`)
- Add the `.cpp` file to `src/CMakeLists.txt` SRCS list

### 4. Register in `RoleFactory`

In `src/RoleFactory.cpp`:

- Include the header
- Add a branch in `createRoleInstance()` matching the type string, passing the needed service references

### 5. Write tests in `test/test_new_role.h`

Create Fake/Mock implementations of any service interfaces the role needs. Follow existing patterns (see `FakeAnalogInput`, `FakeNmea2000Service` in `test/test_fluid_level_sensor_role.h`). Tests should cover:

- `type()` returns correct string
- `configureFromJson()` / `getConfigJson()` round-trip
- `validate()` rejects invalid configs
- `start()` / `loop()` / `stop()` lifecycle behavior
- Enum `ToString` / `FromString` round-trip (if new types added)

### 6. Register tests in `test/main.cpp`

- Include the test header
- Add `RUN_TEST()` calls for each test function

## Current Roles

### FluidLevelSensorRole

Reads analog voltage from tank level sensor, broadcasts as NMEA2000 PGN 127505.

Config: `fluidType`, `inst` (instance), `capacity` (liters), `minVoltage`/`maxVoltage` (calibration)

### WifiGatewayRole

Bridges NMEA2000 data to TCP clients over Wi-Fi (Actisense format). Implements `N2kListenerInterface` to receive bus data.

Config: `ssid`, `password`, `port` (default 10110)

### AisSimulatorRole

Simulates AIS vessel targets on the NMEA2000 bus for testing chart plotters.

Config: `intervalMs` (broadcast interval, default 5000)

## Dependencies

- **NMEA2000** (ttlappalainen) — NMEA 2000 protocol layer; hardware-agnostic, delegates CAN I/O to a concrete subclass
- **NMEA2000_twai** (skarlsson) — provides `tNMEA2000_esp32`, the concrete TWAI hardware driver for the above. Uses the ESP-IDF v2 TWAI API (`twai_driver_install_v2`) rather than ttlappalainen's own `NMEA2000_esp32` adapter, which uses the older single-controller API. The v2 API is required for multi-controller support (ESP32-S3 has 2 TWAI controllers), works cleanly with ESP-IDF 5.x, and has no Arduino framework dependency. Dependency chain: `Nmea2000Service` → `tNMEA2000_esp32` (NMEA2000_twai) → `tNMEA2000` (NMEA2000)
- **ArduinoJson** — JSON parsing/serialization
- **NimBLE-Arduino** (h2zero/esp-nimble-cpp) — Bluetooth Low Energy
- **LittleFS** (joltwallet) — filesystem for role config persistence
- **Unity** — Test framework (native only)
