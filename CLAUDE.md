# YachtMesh Platform

PlatformIO project for ESP32-based NMEA2000 sensor devices. Provides sensor data on the NMEA2000 backbone and can publish NMEA2000 messages over WiFi or other transports.

## Build & Test Commands

```bash
# Build for ESP32
pio run -e esp32dev

# Upload to device
pio run -e esp32dev -t upload

# Monitor serial output
pio device monitor

# Run native tests
pio test -e native
```

## Architecture

### Role Pattern

The project uses a **Role** abstraction (`include/Role.h`) for sensor functionality:

- `configure(RoleConfig&)` - Set up the role with config
- `validate()` - Check if configuration is valid
- `start()` / `stop()` - Control execution
- `loop()` - Called repeatedly in main loop

### Services

Services are injected into Roles via interfaces for testability:

- `Nmea2000ServiceInterface` - Sends metrics to NMEA2000 bus
- `AnalogInputInterface` - Reads analog sensor values

### Metrics

Sensor data flows through the `Metric` struct with:

- `MetricType` enum (e.g., `FluidLevel`)
- `value` - The measurement
- `MetricContext` - Type-specific context (e.g., tank instance, fluid type, capacity)

## Current Roles

### FluidLevelSensorRole

Reads analog voltage from a tank level sensor and broadcasts as NMEA2000 PGN 127505 (Fluid Level).

Configuration:

- `FluidType` - Water, Fuel, GrayWater, BlackWater, etc.
- `inst` - Tank instance number
- `capacity` - Tank capacity in liters
- `minVoltage` / `maxVoltage` - Sensor voltage range for 0-100%

## Project Structure

```
src/
  main.cpp              # Entry point
  FluidLevelSensorRole.cpp
  Nmea2000Service.cpp
  AnalogInputService.cpp
include/
  all.h                 # Common types (Metric, FluidType, interfaces)
  Role.h                # Role base class
  FluidLevelSensorRole.h
  NMEA2000Service.h
test/
  main.cpp              # Test runner
  test_fluid_level_sensor_role.h
```

## Dependencies

- NMEA2000-library - NMEA 2000 protocol implementation
- NMEA2000_esp32 - ESP32 CAN bus driver
- Unity - Test framework (native tests only)

## Hardware

- ESP32 dev board
- CAN transceiver connected to NMEA2000 backbone
- Analog sensors connected to ADC pins
