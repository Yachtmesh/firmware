# Yachtmesh BLE Protocol Specification

**Protocol Version:** `0.4.0`
**Firmware Version:** see the `fw` field of the Device Info characteristic — derived from the git tag of the release build, no longer tracked in this document
**Last Updated:** 2026-07-11

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
| 3 | Device Info | `4e617669-0001-4d65-7368-000000000004` | READ, NOTIFY | Yes |
| 4 | Status | `4e617669-0001-4d65-7368-000000000005` | READ, NOTIFY | Yes |
| 5 | Roles | `4e617669-0001-4d65-7368-000000000006` | READ, NOTIFY | Yes |
| 6 | Config Update | `4e617669-0001-4d65-7368-000000000007` | WRITE | Yes |
| 7 | Factory Reset | `4e617669-0001-4d65-7368-000000000008` | WRITE | Yes |
| 8 | Config Request | `4e617669-0001-4d65-7368-000000000009` | WRITE | Yes |
| 9 | Config Response | `4e617669-0001-4d65-7368-00000000000a` | READ, NOTIFY | Yes |
| 10 | Role Delete | `4e617669-0001-4d65-7368-00000000000b` | WRITE | Yes |
| 11 | Wi-Fi Credentials | `4e617669-0001-4d65-7368-00000000000d` | WRITE | Yes |
| 12 | OTA Control | `4e617669-0001-4d65-7368-00000000000e` | WRITE | Yes |
| 13 | OTA Status | `4e617669-0001-4d65-7368-00000000000f` | READ, NOTIFY | Yes |

`...00c` is reserved for a future "Events" characteristic and intentionally
skipped here.

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

### DeviceInfo Characteristic (JSON)

Read from the **Device Info** characteristic after authentication. Also notified whenever the display name changes.

```json
{
  "id": "HJ1DS2",
  "mac": "aa:bb:cc:dd:ee:ff",
  "nmea": 22,
  "fw": "v1.4.0",
  "displayName": "Sensor Engine Room"
}
```

| Field | Type | Notes |
|-------|------|-------|
| `id` | string | 6-character alphanumeric device ID, fixed to hardware |
| `mac` | string | BT MAC address, colon-separated lowercase hex |
| `nmea` | uint8 | NMEA 2000 bus address |
| `fw` | string | Running firmware version, derived from the git tag of the release build (e.g. `"v1.4.0"`). Non-release builds report a `git describe`-style string (e.g. `"v1.4.0-2-gabc1234"`). This is the version to compare against an OTA release's `version` when deciding whether an update is available. |
| `displayName` | string | User-assigned display label; empty string if never set |

`displayName` is the only field that can change at runtime. The characteristic sends a NOTIFY when it does.

### Status Characteristic (18 bytes)

Notified every 1000ms after authentication. Also readable on demand.

| Bytes | Field | Type | Notes |
|-------|-------|------|-------|
| 0 | Sequence Number | uint8 | Increments each emission, wraps at 255 |
| 1–4 | CPU Temperature | float32 LE | Degrees Celsius |
| 5–8 | Uptime | uint32 LE | Seconds since boot |
| 9–12 | Free Heap | uint32 LE | Current free heap bytes |
| 13–16 | Min Free Heap | uint32 LE | Lowest free heap recorded since boot |
| 17 | CPU Load | uint8 | Approximate CPU load 0–100% |

**Suggested UI:** Show heap in KB. Use `minFreeHeap` as the health indicator — it reflects the worst-case pressure since boot, not just the current snapshot. Suggested bands: >50 KB healthy, 20–50 KB warn, <20 KB critical. CPU load is approximate (idle hook based) — treat as an indicator, not a precise measurement.

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

### Setting the Display Name

The display name is read as part of **Device Info** — no separate request needed. To update it, write JSON to **Config Update** omitting `roleType`:

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

After a successful update, the **Device Info** characteristic sends a NOTIFY with the full updated JSON so connected clients stay in sync without re-reading.

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

## Wi-Fi Credentials & OTA Updates

Over-the-air firmware updates are delivered over Wi-Fi, not BLE — BLE is used
only to supply the device with credentials and a download URL, and to report
progress. The device does one plain HTTPS GET against the exact URL it's
given; it never talks to the GitHub API itself. The client app is expected to
resolve the release it wants (e.g. via the GitHub Releases API or a published
manifest) and hand the device a direct asset URL.

### Wi-Fi Credentials

**Write** JSON to the **Wi-Fi Credentials** characteristic:

```json
{ "ssid": "BoatWifi", "password": "secret123" }
```

This is a **device-level** credential store, separate from any
`WifiGateway`/`WifiGateway0183` role's own `ssid`/`password` config — setting
one does not affect the other. It exists solely so the OTA flow can connect
to Wi-Fi independent of whether a gateway role is configured or currently
running. Persisted in device flash; survives power cycles; cleared by
factory reset.

Response on **Config Response** (reused, same as other write
acknowledgements):

```json
{ "status": "ok" }
```
```json
{ "status": "error", "message": "ssid or password too long" }
```

Constraints: `ssid` up to 32 UTF-8 characters, `password` up to 64 UTF-8
characters, matching ESP-IDF's `wifi_config_t` limits. `ssid` is required and
must be non-empty.

### OTA Control

**Write** JSON to the **OTA Control** characteristic:

```json
{
  "action": "start",
  "manifestUrl": "https://github.com/Yachtmesh/firmware/releases/download/v1.4.0/manifest.json"
}
```

or

```json
{ "action": "cancel" }
```

| Field | Required | Notes |
|-------|----------|-------|
| `action` | Yes | `"start"` or `"cancel"` |
| `manifestUrl` | Yes for `start` | Must be `https://` — plain HTTP is rejected. Points at a release's `manifest.json` asset (see "Wi-Fi Credentials & OTA Updates" above for the manifest shape). |

As of protocol 0.4.0, the client no longer resolves a specific board binary
itself. Firmware fetches `manifestUrl` directly (a plain HTTPS GET, same
`manifest.json` shape a client would use to read `version`/`targets`),
determines its own board (`esp32dev` or `esp32s3`), looks up
`targets[<board>]`, and downloads that target's `file` from the same
directory as `manifestUrl` (GitHub release assets for one tag always share
that directory). `version` in OTA Status is now taken from the fetched
manifest's top-level `version` field, not supplied by the client.
`targets[<board>].sha256` is available to firmware once fetched, but — same
as before — it is **not currently verified** against the downloaded image;
that remains a deliberately deferred future enhancement, not a security
guarantee today.

The write is acknowledged synchronously on **Config Response** (same
`{"status":"ok"}` / `{"status":"error","message":"..."}` shape as other
commands) — this only confirms the command was accepted, not that the update
succeeded. Rejections include: an update already in progress, missing/non-
`https://` `manifestUrl`, or no Wi-Fi credentials configured (write **Wi-Fi
Credentials** first). Once Wi-Fi connects, further failures — manifest fetch
error, invalid manifest JSON, no target for this board — surface as a
`Failed` state on **OTA Status** with a descriptive `message`, not as a
Config Response rejection. Ongoing progress is reported on **OTA Status**.

`cancel` aborts an in-progress connect/manifest-fetch/download/finish and
returns the device to `idle`; it is rejected (as an error ack) if nothing is
in progress.

### OTA Status

**Read** or **subscribe to notifications** on the **OTA Status**
characteristic:

```json
{
  "state": "Downloading",
  "bytesRead": 512000,
  "version": "v1.4.0",
  "currentVersion": "v1.3.2",
  "message": ""
}
```

| Field | Type | Notes |
|-------|------|-------|
| `state` | string | One of `Idle`, `ConnectingWifi`, `Downloading`, `Finishing`, `Success`, `Failed` |
| `bytesRead` | uint32 | Bytes of the firmware image downloaded so far |
| `version` | string | The `version` read from the fetched manifest once resolved; empty until then |
| `currentVersion` | string | The device's currently-running firmware version (same value as Device Info's `fw` field) |
| `message` | string | Empty in normal states; the failure reason when `state` is `Failed` |

Notified on every state transition and periodically during `Downloading` as
bytes accumulate.

**Reboot behavior:** on reaching `Success`, the device reboots
automatically — there is no separate confirm step. A short grace window
(~1.5s) is held after entering `Success` before the actual reboot, so a
subscribed client has a realistic chance to receive the final `Success`
notification before the BLE connection drops for the restart. Clients should
expect the connection to close shortly after observing `state: "Success"`
and should not send further commands during that window.

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

Bridges NMEA 2000 bus traffic to clients in Actisense binary format.

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `ssid` | string | Yes | Wi-Fi network SSID |
| `password` | string | Yes | Wi-Fi network password |
| `port` | uint16 | Yes | Port number (default 10110) |
| `protocol` | string | No | Transport: `"tcp"` (default) or `"udp"`. UDP sends broadcast datagrams to 255.255.255.255 on the configured port. |

### `WifiGateway0183`

Bridges NMEA 2000 bus traffic to clients as NMEA 0183 sentences.

Config fields:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `ssid` | string | Yes | Wi-Fi network SSID |
| `password` | string | Yes | Wi-Fi network password |
| `port` | uint16 | Yes | Port number (default 10110) |
| `protocol` | string | No | Transport: `"tcp"` (default) or `"udp"`. UDP sends broadcast datagrams to 255.255.255.255 on the configured port. |

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

### 0.4.0 — 2026-07-11

- **Breaking:** **OTA Control**'s `start` command no longer takes `url`/`version`/`sha256`. It now takes a single `manifestUrl` pointing at a release's `manifest.json`. Firmware fetches the manifest itself, determines its own board (`esp32dev`/`esp32s3`), and resolves the matching target's download URL — the client no longer needs to know which board a device is running. Rationale: `DeviceInfo` had no way to report board type to clients, so client-side target resolution was either unsafe (guessing) or permanently blocked; firmware already knows its own board at compile time. A client sending the old `{"url":...}` shape after this change gets a `"manifestUrl required"` rejection on **Config Response**.
- **OTA Status**'s `version` field is now sourced from the fetched manifest's top-level `version`, not echoed from the client's command.

### 0.3.0 — 2026-07-10

- Added **Wi-Fi Credentials** (`...00d`), **OTA Control** (`...00e`), and **OTA Status** (`...00f`) characteristics for over-the-air firmware updates over Wi-Fi (triggered/monitored via BLE). Additive — existing characteristics and clients are unaffected.
- `Firmware Version` is no longer hardcoded in this document; it's derived from the release git tag and reported live via Device Info's `fw` field.
- Fixed a transcription error in the characteristics table: **Config Response** and **Role Delete** UUIDs were incorrectly listed as `...000000000010`/`...000000000011` (decimal-looking suffixes); the firmware has always used `...00000000000a`/`...00000000000b` (hex). No wire behavior changed — this was a documentation-only bug.

### 0.2.0 — 2026-04-09

- Status characteristic extended from 9 to 18 bytes: added free heap (bytes 9–12), min free heap (bytes 13–16), CPU load % (byte 17). Additive — clients reading only the first 9 bytes are unaffected.

### 0.1.0 — 2026-03-21

- Initial specification extracted from firmware source (`BluetoothService.h`, `DeviceInfo.h`, `types.h`, `*Role.cpp`)
