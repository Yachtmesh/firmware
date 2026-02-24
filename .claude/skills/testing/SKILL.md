---
name: testing
description: Testing patterns for behavior-driven tests. Use when writing tests or test factories.
---

# Testing Patterns

## Core Principle

**Test behavior, not implementation.** 100% coverage through business behavior, not implementation details.

**Example:** Validation code in `FluidLevelSensorRole.cpp` gets 100% coverage by testing `role.loop()` behavior, NOT by directly testing private helper functions.

---

## Test Through Public API Only

Never test implementation details. Test behavior through the public `Role` interface.

**Why this matters:**
- Tests remain valid when refactoring internals
- Tests document intended business behavior
- Tests catch real bugs, not implementation changes

### Examples

❌ **WRONG - Testing implementation:**
```cpp
// ❌ Testing internal state directly
void test_bad() {
    FluidLevelSensorRole role(manager, nmea);
    role.loop();
    TEST_ASSERT_TRUE(role.calibrated_);   // Private member!
    TEST_ASSERT_EQUAL(42, role.rawAdcValue_);  // Internal detail
}

// ❌ Testing that a private method was called
// (Can't even do this in C++ — if you restructured to allow it, that's a design smell)
```

✅ **CORRECT - Testing behavior through public API:**
```cpp
// ✅ Test what the role emits, not how it computes it
void test_loop_broadcasts_fluid_level_at_midscale() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 1, 200, 1.0f, 5.0f, 0x40, 0.1f};
    role.configure(cfg);
    manager.sensor.reading.current = 3.0f;  // midscale: (3-1)/(5-1) = 50%

    role.start();
    role.loop();

    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, nmea.lastMetric.value);
}

// ✅ Test what validate() enforces, not how it checks
void test_validate_rejects_inverted_current_range() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    FluidLevelConfig cfg{FluidType::Water, 0, 100, 5.0f, 1.0f, 0x40, 0.1f};
    role.configure(cfg);

    TEST_ASSERT_FALSE(role.validate());
}
```

---

## Coverage Through Behavior

Validation code gets 100% coverage by testing the behavior it protects. The goal is to exercise all validation branches via the public `configureFromJson` / `validate` / `loop` chain — never by calling private validators directly.

```cpp
// Tests covering FluidLevelSensorRole validation WITHOUT testing internals

void test_configure_from_json_rejects_inverted_current_range() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    StaticJsonDocument<256> doc;
    doc["fluidType"] = "Water"; doc["inst"] = 0; doc["capacity"] = 100;
    doc["minCurrent"] = 0.02;   // inverted!
    doc["maxCurrent"] = 0.005;
    doc["i2cAddress"] = 0x40;   doc["shuntOhms"] = 0.1;

    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}

void test_configure_from_json_rejects_unknown_fluid_type() {
    // ...
    doc["fluidType"] = "JetFuel";  // not in enum
    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}

void test_configure_from_json_accepts_valid_config() {
    // ...
    doc["fluidType"] = "Water"; doc["minCurrent"] = 0.005; doc["maxCurrent"] = 0.02;
    TEST_ASSERT_TRUE(role.configureFromJson(doc));
}
```

**Key insight:** When a code path isn't covered, ask **"What business behavior am I not testing?"** not "What line am I missing?"

---

## Config Fixture Pattern

The equivalent of the TypeScript factory function. Since C++ has no `Partial<T>`, use a helper function that returns a fully-populated config struct with sensible defaults, plus inline field overrides at the call site.

### Core Principles

1. Return a complete, valid config with sensible defaults
2. Override individual fields inline at the call site
3. Use **real types from `include/`** — never redefine structs in tests
4. Construct fresh objects per test — no shared mutable state

### Basic Pattern

```cpp
// Helper returning a valid default config — place at top of test file
static FluidLevelConfig makeFluidConfig() {
    // Realistic calibration values, not identity values (see mutation-testing skill)
    return FluidLevelConfig{
        FluidType::Water,  // fluidType
        1,                 // inst
        200,               // capacity (liters)
        1.0f,              // minCurrent (A)
        5.0f,              // maxCurrent (A)
        0x40,              // i2cAddress
        0.1f               // shuntOhms
    };
}

// Usage — override only what the test cares about
void test_validate_rejects_zero_capacity() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    auto cfg = makeFluidConfig();
    cfg.capacity = 0;           // ← only field this test cares about
    role.configure(cfg);

    TEST_ASSERT_FALSE(role.validate());
}
```

### JSON Doc Fixture Pattern

For `configureFromJson` tests, use a helper that populates a document with valid defaults:

```cpp
static void fillValidFluidDoc(JsonDocument& doc) {
    doc["type"]       = "FluidLevel";
    doc["fluidType"]  = "Water";
    doc["inst"]       = 1;
    doc["capacity"]   = 200;
    doc["minCurrent"] = 0.005f;
    doc["maxCurrent"] = 0.02f;
    doc["i2cAddress"] = 0x40;
    doc["shuntOhms"]  = 0.1f;
}

// Usage — start valid, then break one field
void test_configure_rejects_missing_capacity() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    StaticJsonDocument<256> doc;
    fillValidFluidDoc(doc);
    doc.remove("capacity");         // ← only change is what the test cares about

    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}
```

### Anti-Patterns

❌ **WRONG: Shared mutable state between tests**
```cpp
// Global or file-scope mutable object — tests can contaminate each other
static FluidLevelSensorRole role(manager, nmea);  // Shared!

void test_first() {
    role.configure(cfg);
    role.start();
    // role is now "started" — affects test_second!
}

void test_second() {
    // role.status().running is already true — not a fresh object!
    TEST_ASSERT_FALSE(role.status().running);  // Fails!
}
```

✅ **CORRECT: Fresh objects per test**
```cpp
void test_first() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);  // Fresh each test
    role.configure(makeFluidConfig());
    role.start();
    TEST_ASSERT_TRUE(role.status().running);
}

void test_second() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);  // Fresh each test
    TEST_ASSERT_FALSE(role.status().running);  // ✅ Passes
}
```

❌ **WRONG: Incomplete or identity-value config**
```cpp
// Identity values hide bugs (see mutation-testing skill)
static FluidLevelConfig makeFluidConfig() {
    return FluidLevelConfig{FluidType::Water, 0, 0, 0.0f, 1.0f, 0x40, 1.0f};
    //                                            ↑   ↑    ↑          ↑
    //  capacity=0 fails validate, min=0 max=1 hides / vs * mutations, shunt=1 hides / vs * mutations
}
```

✅ **CORRECT: Realistic, non-identity defaults**
```cpp
static FluidLevelConfig makeFluidConfig() {
    return FluidLevelConfig{FluidType::Water, 1, 200, 1.0f, 5.0f, 0x40, 0.1f};
    //                                            ↑         ↑     ↑          ↑
    //  capacity>0 passes validate, real calibration range, realistic shunt
}
```

❌ **WRONG: Redefining types or structs in tests**
```cpp
// ❌ Don't shadow or reimport real types
struct FluidLevelConfig {   // Already defined in include/FluidLevelSensorRole.h!
    int inst;
    int capacity;
};
```

✅ **CORRECT: Include and use the real type**
```cpp
#include "FluidLevelSensorRole.h"  // Use real FluidLevelConfig, FluidType, etc.
```

---

## Fake / Mock Pattern

For service dependencies, implement the interface with a test-controllable concrete class. Every service interface (e.g. `Nmea2000ServiceInterface`, `WifiServiceInterface`) gets a `Fake*` or `Mock*` in `test/`.

### Naming Convention

| Type | Name | Purpose |
|------|------|---------|
| `Fake*` | `FakeNmea2000Service` | Records calls and outcomes; simple scripted behavior |
| `Mock*` | `MockCurrentSensorManager` | Controllable via flags (`returnNull`, `reading.current`) |

### Fake Design Rules

1. **Record everything the role might send** — use `bool sent`, `lastMetric`, `sentData`, etc.
2. **Expose controllable inputs** — e.g. `manager.sensor.reading.current = 3.0f`
3. **Implement all interface methods** — even unused ones (`void loop() override {}`)
4. **Keep it simple** — no logic beyond recording calls and returning configured values

```cpp
// Good Fake — records what was sent, controllable return value
class FakeNmea2000Service : public Nmea2000ServiceInterface {
   public:
    bool sent = false;
    Metric lastMetric{MetricType::FluidLevel, 0.0f};
    std::vector<SentMsg> msgsSent;

    void sendMetric(const Metric& metric) override {
        sent = true;
        lastMetric = metric;
    }

    void sendMsg(uint32_t pgn, uint8_t priority,
                 const unsigned char* data, size_t len) override {
        msgsSent.push_back({pgn, priority, std::vector<uint8_t>(data, data + len)});
    }

    void start() override {}
    void loop() override {}
    void addListener(N2kListenerInterface*) override {}
    void removeListener(N2kListenerInterface*) override {}
    uint8_t getAddress() const override { return 22; }
};
```

---

## Coverage Theater Detection

Watch for these patterns that give fake coverage with no real protection.

### Pattern 1: Testing only that a method was called (not what it did)

❌ **WRONG** — `nmea.sent` tells you nothing about correctness:
```cpp
void test_loop_sends_metric() {
    role.loop();
    TEST_ASSERT_TRUE(nmea.sent);  // Only proves sendMetric() was called
}
```

✅ **CORRECT** — Verify the value that was sent:
```cpp
void test_loop_broadcasts_50_percent_at_midscale() {
    manager.sensor.reading.current = 3.0f;  // mid of 1.0–5.0 = 50%
    role.start();
    role.loop();
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_INT((int)MetricType::FluidLevel, (int)nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, nmea.lastMetric.value);
    TEST_ASSERT_EQUAL(1, nmea.lastMetric.context.fluidLevel.inst);
}
```

### Pattern 2: Testing only the happy path (no branch coverage)

❌ **WRONG** — validate() has multiple conditions, only one is tested:
```cpp
void test_validate_passes() {
    role.configure(makeFluidConfig());
    TEST_ASSERT_TRUE(role.validate());  // Only happy path!
}
```

✅ **CORRECT** — Each rejection condition needs its own test:
```cpp
void test_validate_passes_with_valid_config() { ... TEST_ASSERT_TRUE(role.validate()); }
void test_validate_rejects_zero_capacity()    { cfg.capacity = 0; ... TEST_ASSERT_FALSE(role.validate()); }
void test_validate_rejects_inverted_range()   { cfg.minCurrent = 5.0f; cfg.maxCurrent = 1.0f; ... TEST_ASSERT_FALSE(role.validate()); }
void test_validate_rejects_equal_range()      { cfg.minCurrent = 3.0f; cfg.maxCurrent = 3.0f; ... TEST_ASSERT_FALSE(role.validate()); }
```

### Pattern 3: Testing configureFromJson success but not what was configured

❌ **WRONG** — returns true but didn't verify fields were parsed:
```cpp
void test_configure_from_json() {
    TEST_ASSERT_TRUE(role.configureFromJson(doc));  // Doesn't verify fields!
}
```

✅ **CORRECT** — Verify the parsed values are correct:
```cpp
void test_configure_from_json_parses_all_fields() {
    TEST_ASSERT_TRUE(role.configureFromJson(doc));
    TEST_ASSERT_EQUAL(FluidType::Water, role.config.fluidType);
    TEST_ASSERT_EQUAL(1, role.config.inst);
    TEST_ASSERT_EQUAL(200, role.config.capacity);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.005f, role.config.minCurrent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.02f,  role.config.maxCurrent);
}
```

### Pattern 4: Testing trivial `type()` without asserting the exact string

❌ **WRONG** — Non-null check barely tests anything:
```cpp
TEST_ASSERT_NOT_NULL(role.type());
```

✅ **CORRECT** — Assert the exact registered string:
```cpp
TEST_ASSERT_EQUAL_STRING("FluidLevel", role.type());
```

---

## No 1:1 Mapping Between Test Files and Implementation Files

Test files should reflect behavioral units (a Role, a Service), not mirror every `.cpp` file.

❌ **WRONG:**
```
src/
  FluidLevelSensorRole.cpp
  FluidLevelValidator.cpp    ← internal helper
  FluidLevelFormatter.cpp    ← internal helper
test/
  test_fluid_level_validator.h   ← 1:1 with internal file
  test_fluid_level_formatter.h   ← 1:1 with internal file
```

✅ **CORRECT:**
```
src/
  FluidLevelSensorRole.cpp
  FluidLevelValidator.cpp    ← internal; covered by behavior tests
  FluidLevelFormatter.cpp    ← internal; covered by behavior tests
test/
  test_fluid_level_sensor_role.h  ← Tests Role behavior end-to-end
```

**Why:** Validator and formatter logic gets 100% coverage through the Role's public API. Tests survive internal restructuring because they only depend on the Role interface.

---

## Enum Round-Trip Tests

Every new enum type must have string conversion tests. These are the only tests that are allowed to be 1:1 with the enum values — because they directly test the macro-generated serialization contract.

```cpp
void test_fluid_type_to_string() {
    TEST_ASSERT_EQUAL_STRING("Fuel",     FluidTypeToString(FluidType::Fuel));
    TEST_ASSERT_EQUAL_STRING("Water",    FluidTypeToString(FluidType::Water));
    TEST_ASSERT_EQUAL_STRING("Unavailable", FluidTypeToString(FluidType::Unavailable));
    // ... all values
}

void test_fluid_type_from_string() {
    TEST_ASSERT_EQUAL(FluidType::Fuel,  FluidTypeFromString("Fuel"));
    TEST_ASSERT_EQUAL(FluidType::Water, FluidTypeFromString("Water"));
}

// Unknown string must return Unavailable (not crash, not assert)
void test_fluid_type_from_string_unknown_returns_unavailable() {
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("JetFuel"));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString(""));
    TEST_ASSERT_EQUAL(FluidType::Unavailable, FluidTypeFromString("fuel"));  // case-sensitive
}

// Round-trip: serialize then deserialize must produce original value
void test_fluid_type_round_trip() {
    TEST_ASSERT_EQUAL(FluidType::Fuel, FluidTypeFromString(FluidTypeToString(FluidType::Fuel)));
    // ... all values
}
```

---

## JSON Round-Trip Tests

Every Role must have a `getConfigJson` / `configureFromJson` round-trip test. This is the persistence contract — it must be exact.

```cpp
void test_fluid_level_role_json_round_trip() {
    MockCurrentSensorManager manager;
    FakeNmea2000Service nmea;
    FluidLevelSensorRole role(manager, nmea);

    // Start with known config
    FluidLevelConfig cfg{FluidType::Water, 2, 150, 0.005f, 0.02f, 0x41, 0.1f};
    role.configure(cfg);

    // Serialize
    StaticJsonDocument<256> doc;
    role.getConfigJson(doc);

    // Verify every field
    TEST_ASSERT_EQUAL_STRING("FluidLevel", doc["type"]);
    TEST_ASSERT_EQUAL_STRING("Water",      doc["fluidType"]);
    TEST_ASSERT_EQUAL(2,    doc["inst"]);
    TEST_ASSERT_EQUAL(150,  doc["capacity"]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.005f, doc["minCurrent"]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.02f,  doc["maxCurrent"]);
    TEST_ASSERT_EQUAL(0x41, doc["i2cAddress"]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,  0.1f,   doc["shuntOhms"]);
}
```

---

## Summary Checklist

When writing tests, verify:

- [ ] Testing behavior through public `Role`/service API (not private members or helpers)
- [ ] No tests of internal state or private methods
- [ ] Fresh objects constructed per test (no shared mutable state)
- [ ] Config fixtures use real types from `include/` (not redefined)
- [ ] Fixture defaults use non-identity values (see mutation-testing skill)
- [ ] Each validation rejection condition has its own test
- [ ] `configureFromJson` tests verify parsed field values, not just return value
- [ ] `type()` tested with `TEST_ASSERT_EQUAL_STRING` for exact string
- [ ] Enum `ToString`/`FromString` round-trips covered for every value
- [ ] Unknown enum string tested (must return `Unavailable`, not crash)
- [ ] JSON round-trip asserts every field, not just a subset
- [ ] `loop()` tests assert what was sent, not just that *something* was sent
- [ ] No 1:1 mapping between test files and implementation files
