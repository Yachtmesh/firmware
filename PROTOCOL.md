# Yachtmesh BLE Protocol Specification

**Protocol Version:** `0.2.0`
**Firmware Version:** `0.1.0`
**Last Updated:** 2026-04-08

This document is the canonical specification for the Bluetooth Low Energy protocol between Yachtmesh firmware and client applications (iOS, Android, third-party). It is maintained in the firmware repository because the firmware is the GATT server and the authoritative definer of the protocol.

Client implementors should watch this file for changes. Any change to a characteristic UUID, binary format, JSON field name, or enum value is a **breaking change** unless explicitly marked as additive.

---

## Versioning

This protocol uses semantic versioning:

- **MAJOR** bump: Removed or renamed characteristic, changed binary format, removed JSON field, removed enum value
- **MINOR** bump: New characteristic, new JSON field (optional), new enum value, new role type
- **PATCH** bump: Bug fix in firmware behaviour with no wire format change

The current protocol version is encoded in `PROTOCOL_VERSION` at the top of this file. Client implementations should surface the firmware version (available in the DeviceInfo characteristic) so that incompatibilities can be diagnosed.

---

## BLE Advertising

**Device name prefix:** `Yachtmesh-` followed by a 6-character alphanumeric device ID
**Example:** `Yachtmesh-HJ1DS2`

Clients should filter discovered peripherals by this name prefix.

---

## GATT Service

**Service UUID:** `4e617669-0001-4d65-7368-000000000001`

All characteristics belong to this single service.

---

## Characteristics

| # | Name | UUID | Properties | Auth Required |
|---|------|------|------------|---------------|
| 1 | Password | `4e617669-0001-4d65-7368-000000000002` | WRITE | No |
| 2 | Auth Status | `4e617669-0001-4d65-7368-000000000003` | READ, NOTIFY | No |
| 3 | Device Info | `4e617669-0001-4d65-7368-000000000004` | READ | Yes |
| 4 | Status | `4e617669-0001-4d65-7368-000000000005` | READ, NOTIFY | Yes |
| 5 | Roles | `4e617669-0001-4d65-7368-000000000006` | READ, NOTIFY | Yes |
| 6 | Config Update | `4e617669-0001-4d65-7368-000000000007` | WRITE | Yes |
| 7 | Factory Reset | `4e617669-0001-4d65-7368-000000000008` | WRITE | Yes |
| 8 | Config Request | `4e617669-0001-4d65-7368-000000000009` | WRITE | Yes |
| 9 | Config Response | `4e617669-0001-4d65-7368-000000000010` | READ, NOTIFY | Yes |
| 10 | Role Delete | `4e617669-0001-4d65-7368-000000000011` | WRITE | Yes |

---

## Authentication Flow

Authentication is per-connection and must be completed before accessing any characteristic marked "Auth Required".

1. Client writes UTF-8 password string to **Password** characteristic
2. Client subscribes to notifications on **Auth Status** characteristic (or polls by reading)
3. Firmware responds with a single byte:
   - `0x01` — authenticated successfully
   - `0x00` — authentication failed
4. On disconnect, the authentication state is cleared for that connection handle

**Default password:** `yachtmesh123` (configurable at compile time via `DEFAULT_BLE_PASSWORD`)

---

## Binary Formats

### DeviceInfo Characteristic (20 bytes)

Read from the **Device Info** characteristic after authentication.

| Bytes | Field | Type | Notes |
|-------|-------|------|-------|
| 0–5 | Device ID | 6-byte ASCII | e.g. `HJ1DS2` |
| 6–11 | MAC Address | 6 raw bytes | Big-endian MAC |
| 12 | NMEA 2000 Address | uint8 | |
| 13 | Firmware Version Major | uint8 | |
| 14 | Firmware Version Minor | uint8 | |
| 15 | Firmware Version Patch | uint8 | |
| 16–19 | Reserved | — | Always zero |

### Status Characteristic (9 bytes)

Notified every 1000ms after authentication. Also readable on demand.

| Bytes | Field | Type | Notes |
|-------|-------|------|-------|
| 0 | Sequence Number | uint8 | Increments each emission, wraps at 255 |
| 1–4 | CPU Temperature | float32 LE | Degrees Celsius |
| 5–8 | Uptime | uint32 LE | Seconds since boot |

---

## JSON Protocol

All JSON messages are UTF-8 encoded. The firmware uses a 512-byte static JSON document buffer (`StaticJsonDocument<512>`), so payloads written to the device must not exceed ~500 bytes.

### Reading the Roles List

**Read** the **Roles** characteristic. Returns a JSON array:

```json
[
  { "id": "FluidLevel-abc", "type": "FluidLevel", "status": { "running": true } },
  { "id": "WifiGateway-xyz", "type": "WifiGateway", "status": { "running": true, "ip": "192.168.1.10" } }
]
```

Fields:

| Field | Type | Notes |
|-------|------|-------|
| `id` | string | Role instance ID, format `<Type>-<random>` |
| `type` | string | Role type string — see Role Types section |
| `status.running` | bool | Whether the role is active |
| `status.ip` | string | WifiGateway/WifiGateway0183 only — assigned IP address |

### Reading a Role's Config

1. **Write** the role's `id` string (UTF-8) to **Config Request** characteristic
2. **Read or await notification** on **Config Response** characteristic

Response:

```json
{ "id": "FluidLevel-abc", "type": "FluidLevel", "config": { ... } }
```

The `config` object is role-specific — see Role Types section.

### Reading Device Config

Device-level config (currently display name) is read via the same **Config Request / Config Response** pair as role configs, using the reserved sentinel key `__device__`.

Role IDs always follow the `<Type>-<alphanumeric>` format, so `__device__` can never collide with a real role ID.

1. **Write** the string `__device__` (UTF-8) to **Config Request** characteristic
2. **Read or await notification** on **Config Response** characteristic

Response:

```json
{ "displayName": "Sensor Engine Room" }
```

`displayName` is an empty string if never set:

```json
{ "displayName": "" }
```

### Setting the Display Name

**Write** JSON to **Config Update** characteristic, omitting `roleType`:

```json
{ "displayName": "Sensor Engine Room" }
```

The firmware distinguishes this from a role config update by the absence of `roleType`. Response on **Config Response**:

```json
{ "status": "ok" }
```

```json
{ "status": "error", "message": "displayName exceeds 64 characters" }
```

Constraints:
- Maximum 64 UTF-8 characters
- Write an empty string to clear: `{ "displayName": "" }`
- Persisted in device flash; survives power cycles
- Cleared by factory reset

### Creating or Updating a Role

**Write** JSON to **Config Update** characteristic:

```json
{ "roleId": "FluidLevel-abc", "roleType": "FluidLevel", "config": { ... } }
```

| Field | Required | Notes |
|-------|----------|-------|
| `roleId` | No | Omit or send empty string to **create** a new role; provide existing ID to **update** |
| `roleType` | Yes | Must match a valid role type string |
| `config` | Yes | Role-specific config object |

### Deleting a Role

**Write** the role's `id` string (UTF-8) to **Role Delete** characteristic.

### Factory Reset

**Write** any value (conventionally `0x01`) to **Factory Reset** characteristic. All roles and configuration are erased.

---

## Role Types

Role type strings are case-sensitive.

### `FluidLevel`

Reads an analog tank level sensor and broadcasts NMEA 2000 PGN 127505.

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `fluidType` | string | Yes | See FluidType enum |
| `inst` | uint8 | Yes | NMEA 2000 instance number |
| `capacity` | uint16 | Yes | Tank capacity in litres |
| `minVoltage` | float | Yes | Calibration: sensor voltage at empty |
| `maxVoltage` | float | Yes | Calibration: sensor voltage at full |

### `WifiGateway`

Bridges NMEA 2000 bus traffic to TCP clients in Actisense format.

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `ssid` | string | Yes | Wi-Fi network SSID |
| `password` | string | Yes | Wi-Fi network password |
| `port` | uint16 | Yes | TCP listen port (default 10110) |

### `WifiGateway0183`

Same config as `WifiGateway`. Bridges NMEA 0183 sentences instead of Actisense binary.

### `AisSimulator`

Emits simulated AIS vessel targets on the NMEA 2000 bus (for testing chart plotters).

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `intervalMs` | uint32 | Yes | Broadcast interval in milliseconds (default 5000) |

### `WeatherStation`

Reads a BME280 temperature/humidity/pressure sensor and broadcasts NMEA 2000 PGN 130311.

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `inst` | uint8 | Yes | NMEA 2000 instance number (default 0) |
| `intervalMs` | uint32 | Yes | Broadcast interval in milliseconds (default 2500) |

---

## Enums

### FluidType

Used in `FluidLevel` role config. Values are serialised as exact string names.

| Value | Notes |
|-------|-------|
| `Fuel` | |
| `Water` | |
| `GrayWater` | |
| `LiveWell` | |
| `Oil` | |
| `BlackWater` | |
| `FuelGasoline` | |
| `Error` | Not a valid config value — indicates sensor error |
| `Unavailable` | Not a valid config value — firmware fallback for unknown strings |

---

## Known Issues / Discrepancies

| # | Description | Status |
|---|-------------|--------|
| 1 | `FluidLevel` config field `minVoltage`/`maxVoltage` is named `minCurrent`/`maxCurrent` in the firmware struct (`FluidLevelSensorConfig`). The JSON wire format uses `minVoltage`/`maxVoltage` (matching this spec). The struct naming is a firmware-internal inconsistency, not a protocol issue. | Open |

---

## Changelog

### 0.2.0 — 2026-04-08

- Added display name: read via `__device__` sentinel on Config Request, write via Config Update with `displayName` field and no `roleType`
- Display name is cleared by factory reset

### 0.1.0 — 2026-03-21

- Initial specification extracted from firmware source (`BluetoothService.h`, `DeviceInfo.h`, `types.h`, `*Role.cpp`)
