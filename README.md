# Yachtmesh Firmware

ESP32/ESP32-S3 firmware for the Yachtmesh NMEA 2000 platform. Configures via Bluetooth Low Energy from the Yachtmesh iOS app (or any BLE client following the protocol).

The idea is simple: a small device plugs into your boat's NMEA 2000 backbone and exposes its data over Wi-Fi and BLE — no proprietary gateways, no locked-down apps, no sky-high prices. You own the hardware, you own the code, you decide what runs on it.

---

## What it does

The firmware is built around a **role** system. Each role is an independent function the device can perform. You add and configure roles from the mobile app over BLE. Roles persist across reboots.

| Role | What it does |
|------|-------------|
| `FluidLevel` | Reads an analog tank sensor and broadcasts NMEA 2000 PGN 127505 (fluid level) |
| `WifiGateway` | Bridges NMEA 2000 traffic to TCP clients in Actisense binary format |
| `WifiGateway0183` | Same, but as NMEA 0183 sentences — useful for chart plotters and Signal K |
| `WeatherStation` | Reads a BME280 sensor and broadcasts NMEA 2000 PGN 130311 (temperature/humidity/pressure) |
| `AisSimulator` | Emits simulated AIS targets on the bus — handy for testing chart plotters without going offshore |
| `VeDirectBattery` | Reads Victron VE.Direct serial output and puts battery data on the NMEA 2000 bus |

Multiple roles can run simultaneously. A typical setup might be a fluid level sensor, a weather station, and a Wi-Fi gateway all running on the same device.

---

## Hardware

Targets two boards out of the box:

- **ESP32-S3 DevKitC-1** (`esp32s3` env) — recommended, more RAM, USB-native
- **ESP32 DevKit** (`esp32dev` env) — original ESP32, still fully supported

Pin assignments are set in `platformio.ini` via build flags:

| Signal | ESP32 | ESP32-S3 |
|--------|-------|----------|
| TWAI TX (NMEA 2000 CAN) | GPIO 5 | GPIO 5 |
| TWAI RX (NMEA 2000 CAN) | GPIO 4 | GPIO 4 |
| I2C SDA (BME280 etc.) | GPIO 21 | GPIO 8 |
| I2C SCL | GPIO 22 | GPIO 9 |

You'll need a CAN transceiver (e.g. SN65HVD230) between the ESP32 and the NMEA 2000 bus.

---

## Building and flashing

Install [PlatformIO](https://platformio.org/) then:

```bash
# Build for ESP32-S3
pio run -e esp32s3

# Flash
pio run -e esp32s3 --target upload

# Monitor serial output
pio device monitor

# Run native unit tests (no hardware required)
pio test -e native
```

The default BLE password is `yachtmesh123`. Override it at build time:

```ini
build_flags = -DDEFAULT_BLE_PASSWORD=\"yourpassword\"
```

---

## Connecting

1. Power the device — it advertises as `Yachtmesh-XXXXXX` over BLE
2. Connect with the Yachtmesh iOS app (or any BLE client)
3. Authenticate with the password
4. Add and configure roles

The BLE protocol is fully documented in [`PROTOCOL.md`](PROTOCOL.md). If you want to build your own client, everything you need is there — UUIDs, binary formats, JSON schemas, auth flow.

---

## Architecture

### Role pattern

Every sensor function is a `Role` subclass with a simple lifecycle:

```cpp
class Role {
    virtual void configure(const RoleConfig& cfg) = 0;
    virtual bool configureFromJson(const JsonDocument& doc) = 0;
    virtual void start() = 0;
    virtual void loop() = 0;
    virtual void stop() = 0;
    virtual RoleStatus status() = 0;
};
```

Roles are created and destroyed at runtime via BLE commands. Config is persisted as JSON on LittleFS under `/roles/`. The `RoleManager` handles lifecycle and defers filesystem writes out of BLE callbacks.

### Service interfaces

Roles never touch ESP32 hardware directly. Platform functions (NMEA 2000, Wi-Fi, ADC, TCP) are injected as abstract interfaces. This keeps roles testable on a native host without any embedded toolchain.

```
Role → Nmea2000ServiceInterface → Nmea2000Service → tNMEA2000 (CAN/TWAI)
     → WifiServiceInterface    → WifiService
     → AnalogInputInterface    → AnalogInputService (ADC)
     → TcpServerInterface      → TcpServer
```

### Adding a new role

See `CLAUDE.md` for the full step-by-step. The short version:

1. Add a config struct and role class in `include/`
2. Implement in `src/`
3. Register in `RoleFactory`
4. Write tests in `test/`

### Testing

Unit tests run natively (no ESP32 required) using the Unity framework. Hardware interfaces are replaced with mock/fake implementations. Run with `pio test -e native`.

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [NMEA2000](https://github.com/ttlappalainen/NMEA2000) | NMEA 2000 protocol layer |
| [NMEA2000_twai](https://github.com/skarlsson/NMEA2000_twai) | ESP32 CAN/TWAI driver for the above |
| [ArduinoJson](https://arduinojson.org/) | JSON parsing and serialisation |
| [NimBLE-Arduino / esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Bluetooth Low Energy |
| [LittleFS](https://github.com/joltwallet/esp_littlefs) | Filesystem for role config persistence |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | Unit test framework (native only) |

---

## Contributing

Pull requests are welcome. A few things that would be useful:

- New role types (battery monitors, engine data, AIS receiver, NMEA 0183 input)
- Support for additional analog sensors
- Signal K or MQTT output
- Android client

If you're adding a role, write tests first — the project follows TDD and the CI will run `pio test -e native` on every PR.

Open an issue if something doesn't work or you have a question about the protocol or architecture.

---

## License

MIT — do what you want with it, just keep the attribution.

---

Built by sailors, for sailors. If you're running this on your boat, we'd love to hear about it.
