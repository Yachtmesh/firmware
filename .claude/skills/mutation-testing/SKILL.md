---
name: mutation-testing
description: Mutation testing patterns for verifying test effectiveness. Use when analyzing branch code to find weak or missing tests.
---

# Mutation Testing

Mutation testing answers the question: **"Are my tests actually catching bugs?"**

Code coverage tells you what code your tests execute. Mutation testing tells you if your tests would **detect changes** to that code. A test suite with 100% coverage can still miss 40% of potential bugs.

---

## Core Concept

**The Mutation Testing Process:**

1. **Generate mutants**: Introduce small bugs (mutations) into production code
2. **Run tests**: Execute your test suite against each mutant (`pio test -e native`)
3. **Evaluate results**: If tests fail, the mutant is "killed" (good). If tests pass, the mutant "survived" (bad - your tests missed the bug)

**The Insight**: A surviving mutant represents a bug your tests wouldn't catch.

---

## When to Use This Skill

Use mutation testing analysis when:

- Reviewing code changes on a branch
- Verifying test effectiveness after TDD
- Identifying weak tests that appear to have coverage
- Finding missing edge case tests
- Validating that refactoring didn't weaken test suite

**Integration with TDD:**

```
TDD Workflow                    Mutation Testing Validation
┌─────────────────┐             ┌─────────────────────────────┐
│ RED: Write test │             │                             │
│ GREEN: Pass it  │──────────►  │ After GREEN: Verify tests   │
│ REFACTOR        │             │ would kill relevant mutants │
└─────────────────┘             └─────────────────────────────┘
```

---

## Systematic Branch Analysis Process

When analyzing code on a branch, follow this systematic process:

### Step 1: Identify Changed Code

```bash
# Get files changed on the branch
git diff main...HEAD --name-only | grep -E '\.(cpp|h)$' | grep -v 'test_\|Mock\|Fake'

# Get detailed diff for analysis
git diff main...HEAD -- src/ include/
```

### Step 2: Generate Mental Mutants

For each changed function/method, mentally apply mutation operators (see Mutation Operators section below).

### Step 3: Verify Test Coverage

For each potential mutant, ask:

1. **Is there a test that exercises this code path?**
2. **Would that test FAIL if this mutation were applied?**
3. **Is the assertion specific enough to catch this change?**

### Step 4: Document Findings

Categorize findings:

| Category | Description | Action Required |
|----------|-------------|-----------------|
| Killed | Test would fail if mutant applied | None - tests are effective |
| Survived | Test would pass with mutant | Add/strengthen test |
| No Coverage | No test exercises this code | Add behavior test |
| Equivalent | Mutant produces same behavior | None - not a real bug |

---

## Mutation Operators

### Arithmetic Operator Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `a + b` | `a - b` | Addition behavior |
| `a - b` | `a + b` | Subtraction behavior |
| `a * b` | `a / b` | Multiplication behavior |
| `a / b` | `a * b` | Division behavior |
| `a % b` | `a * b` | Modulo behavior |

**Example Analysis:**

```cpp
// Production code (e.g. FluidLevelSensorRole.cpp)
float calculatePercent(float current, float minCurrent, float maxCurrent) {
    return (current - minCurrent) / (maxCurrent - minCurrent);
}

// Mutant: (current - minCurrent) * (maxCurrent - minCurrent)
// Question: Would tests fail if / became *?

// ❌ WEAK TEST - Would NOT catch mutant
void test_percent_calculation() {
    // min=0, max=1 → denominator is 1 → divide or multiply give same result!
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calculatePercent(0.5f, 0.0f, 1.0f));
}

// ✅ STRONG TEST - Would catch mutant
void test_percent_calculation() {
    // min=1.0, max=5.0, current=3.0 → (3-1)/(5-1) = 0.5
    // mutant:                        → (3-1)*(5-1) = 8.0  (DIFFERENT!)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calculatePercent(3.0f, 1.0f, 5.0f));
}
```

### Conditional Expression Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `a < b` | `a <= b` | Boundary value at equality |
| `a < b` | `a >= b` | Both sides of condition |
| `a <= b` | `a < b` | Boundary value at equality |
| `a <= b` | `a > b` | Both sides of condition |
| `a > b` | `a >= b` | Boundary value at equality |
| `a >= b` | `a > b` | Boundary value at equality |

**Example Analysis:**

```cpp
// Production code
bool validate() {
    return config.capacity > 0 && config.minCurrent < config.maxCurrent;
}

// Mutant: config.minCurrent <= config.maxCurrent
// Question: Would tests fail if < became <=?

// ❌ WEAK TEST - Would NOT catch boundary mutant
void test_validate_rejects_inverted_range() {
    cfg.minCurrent = 5.0f;
    cfg.maxCurrent = 1.0f;  // Inverted — both < and <= catch this
    TEST_ASSERT_FALSE(role.validate());
}

// ✅ STRONG TEST - Would catch boundary mutant
void test_validate_rejects_equal_range() {
    cfg.minCurrent = 3.0f;
    cfg.maxCurrent = 3.0f;  // Equal — < rejects, <= accepts (DIFFERENT!)
    TEST_ASSERT_FALSE(role.validate());
}
```

### Equality Operator Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `a == b` | `a != b` | Both equal and not equal cases |
| `a != b` | `a == b` | Both equal and not equal cases |

**Example Analysis:**

```cpp
// Production code — enum FromString returns Unavailable for unknown strings
FluidType type = FluidTypeFromString(str);
if (type == FluidType::Unavailable) return false;

// Mutant: type != FluidType::Unavailable
// ❌ WEAK TEST - Only tests valid strings (condition rarely fires)
// ✅ STRONG TEST - Tests both valid AND unknown string
void test_configure_rejects_unknown_fluid_type() {
    doc["fluidType"] = "Diesel";       // valid — must succeed
    TEST_ASSERT_TRUE(role.configureFromJson(doc));
    doc["fluidType"] = "JetFuel";      // unknown — must fail
    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}
```

### Logical Operator Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `a && b` | `a \|\| b` | Case where one is true, other is false |
| `a \|\| b` | `a && b` | Case where one is true, other is false |

**Example Analysis:**

```cpp
// Production code
bool validate() {
    return config.capacity > 0 && config.minCurrent < config.maxCurrent;
}

// Mutant: config.capacity > 0 || config.minCurrent < config.maxCurrent
// Question: Would tests fail if && became ||?

// ❌ WEAK TEST - Only checks when both fail
void test_validate_both_bad() {
    cfg.capacity = 0;
    cfg.minCurrent = 5.0f; cfg.maxCurrent = 1.0f;
    TEST_ASSERT_FALSE(role.validate());  // || also returns false here (SAME!)
}

// ✅ STRONG TEST - Checks each condition independently
void test_validate_rejects_zero_capacity() {
    cfg.capacity = 0;
    cfg.minCurrent = 1.0f; cfg.maxCurrent = 5.0f;  // range is valid
    TEST_ASSERT_FALSE(role.validate());  // && = false, || = true (DIFFERENT!)
}

void test_validate_rejects_inverted_current_range() {
    cfg.capacity = 200;                               // capacity is valid
    cfg.minCurrent = 5.0f; cfg.maxCurrent = 1.0f;
    TEST_ASSERT_FALSE(role.validate());  // && = false, || = true (DIFFERENT!)
}
```

### Boolean Literal Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `return true` | `return false` | Both success and failure paths |
| `return false` | `return true` | Both success and failure paths |
| `!condition` | `condition` | Negation is necessary |

### Block Statement Mutations (Remove Statement)

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `{ code; }` | `{ }` | Side effects of the block |
| `doSomething();` | *(removed)* | Call was necessary |

**Example Analysis:**

```cpp
// Production code
void loop() {
    auto reading = analog_.read();
    float percent = calcPercent(reading.voltage);
    nmea_.sendMetric({MetricType::FluidLevel, percent});
}

// Mutant: Empty loop() — sendMetric() call removed
// ❌ WEAK TEST - Would NOT catch mutant
void test_loop_does_not_crash() {
    role.loop();
    // No assertion — empty loop() also "doesn't crash"!
}

// ✅ STRONG TEST - Verifies the observable side effect
void test_loop_broadcasts_fluid_level() {
    manager.sensor.reading.current = 3.0f;
    role.loop();
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, nmea.lastMetric.value);
}
```

### String Literal Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `"FluidLevel"` | `""` | Non-empty string returned |
| `"FluidLevel"` | `"AisSimulator"` | Exact string value |

**Example Analysis:**

```cpp
// Production code
const char* type() override { return "FluidLevel"; }

// ✅ STRONG TEST - Verifies exact string, not just non-null
void test_type_string() {
    TEST_ASSERT_EQUAL_STRING("FluidLevel", role.type());
}
```

### Arithmetic Assignment Mutations

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `a += b` | `a -= b` | Accumulation direction |
| `a -= b` | `a += b` | Reduction direction |
| `a *= b` | `a /= b` | Scale direction |
| `++i` | `--i` | Loop iteration direction |
| `i++` | `i--` | Loop iteration direction |

### Pointer / Reference Mutations (C++ specific)

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `nullptr` check | removed | Null is handled correctly |
| `*ptr` | `ptr` | Dereference is necessary |
| `ptr->field` | `ptr->otherField` | Correct field accessed |

**Example Analysis:**

```cpp
// Production code
bool configureFromJson(const JsonDocument& doc) {
    if (!doc["type"].is<const char*>()) return false;
    // ...
}

// Mutant: Remove the null check — always proceeds
// ❌ WEAK TEST - Only passes well-formed JSON
// ✅ STRONG TEST - Tests missing field
void test_configure_rejects_missing_type_field() {
    JsonDocument doc;
    doc["capacity"] = 200;  // "type" field missing
    TEST_ASSERT_FALSE(role.configureFromJson(doc));
}
```

### Enum/Type Mutations (project-specific)

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `FluidType::Fuel` | `FluidType::Water` | Correct type persisted |
| `MetricType::FluidLevel` | `MetricType::Temperature` | Correct metric sent |

**Example Analysis:**

```cpp
// Production code — round-trip via JSON
// ✅ STRONG TEST - Verifies exact enum value survives serialize/deserialize
void test_fluid_type_round_trips_through_json() {
    cfg.fluidType = FluidType::FuelGasoline;
    role.configure(cfg);
    JsonDocument doc;
    role.getConfigJson(doc);
    TEST_ASSERT_EQUAL_STRING("FuelGasoline", doc["fluidType"].as<const char*>());
}
```

### Calibration / Scaling Mutations (register math)

For sensor services that compute register values from physical constants:

| Original | Mutated | Test Should Verify |
|----------|---------|-------------------|
| `trunc(0.04096 / (lsb * shunt))` | `round(...)` | Truncation vs rounding |
| `>> 3` | `>> 2` | Correct bit shift |
| `<< 8` | `<< 4` | Correct byte assembly |

**Example Analysis (INA219 calibration):**

```cpp
// ❌ WEAK TEST - Any non-zero value passes
TEST_ASSERT_TRUE(cal > 0);

// ✅ STRONG TEST - Exact register value from datasheet formula
// Cal = trunc(0.04096 / (0.001 * 0.1)) = trunc(409.6) = 409
TEST_ASSERT_EQUAL_UINT16(409, cal);
```

---

## Mutant States and Metrics

### Mutant States

| State | Meaning | Action |
|-------|---------|--------|
| **Killed** | Test failed when mutant applied | Good - tests are effective |
| **Survived** | Tests passed with mutant active | Bad - add/strengthen test |
| **No Coverage** | No test exercises this code | Add behavior test |
| **Equivalent** | Mutant produces same behavior | No action - not a real bug |

### Metrics

- **Mutation Score**: `killed / valid * 100` - The higher, the better
- **Detected**: `killed`
- **Undetected**: `survived + no coverage`

### Target Mutation Score

| Score | Quality |
|-------|---------|
| < 60% | Weak test suite - significant gaps |
| 60-80% | Moderate - many improvements possible |
| 80-90% | Good - but still gaps to address |
| > 90% | Strong - watch for equivalent mutants |

---

## Equivalent Mutants

Equivalent mutants produce the same behavior as the original code. They cannot be killed because there is no observable difference.

### Common Equivalent Mutant Patterns in this codebase

**Pattern 1: Identity elements in math**

```cpp
// Mutating += 0 to -= 0 makes no difference
voltage += 0.0f;  // Can mutate to -= 0.0f — equivalent!
```

**Pattern 2: Boundary conditions where both sides produce the same result**

```cpp
// When minCurrent == maxCurrent, dividing by zero anyway — condition doesn't matter
if (maxCurrent > minCurrent) { ... }
// Mutating > to >= just changes whether the guard triggers at equality
// — only distinguishable if you test with equal values
```

**Pattern 3: Calibration computed at construction only**

```cpp
// If calibration only runs once and we verify the written value,
// mutating the "only once" guard to "always" doesn't change test outcome
// because the test only calls read() once.
```

### How to Handle Equivalent Mutants

1. **Identify**: Analyze if mutation truly changes observable behavior
2. **Document**: Note why mutant is equivalent
3. **Accept**: 100% mutation score may not be achievable
4. **Consider refactoring**: Sometimes equivalent mutants indicate unclear code

---

## Branch Analysis Checklist

When analyzing code changes on a branch:

### For Each Function/Method Changed:

- [ ] **Arithmetic operators**: Would changing +, -, *, / be detected?
- [ ] **Conditionals**: Are boundary values tested (>=, <=, exact values)?
- [ ] **Boolean logic**: Are all branches of &&, || tested independently?
- [ ] **Return statements**: Would changing `return true` / `return false` be detected?
- [ ] **Method calls**: Would removing a call (sendMetric, write, etc.) be detected?
- [ ] **String literals**: Would wrong or empty `type()` string be detected?
- [ ] **Enum values**: Would a wrong enum value survive a JSON round-trip test?
- [ ] **Register math**: Are exact computed register values asserted (not just "non-zero")?
- [ ] **Pointer safety**: Are null/missing-field paths exercised?

### Red Flags (Likely Surviving Mutants):

- [ ] Tests only verify "no crash / no throw"
- [ ] Tests only check one side of a condition
- [ ] Tests use identity values (0.0, 1.0, empty string, `min=0 max=1`)
- [ ] Tests verify a mock *was called* but not *what it was called with*
- [ ] Tests don't verify return values of `configureFromJson` / `validate`
- [ ] Boundary values not tested (e.g. only test `capacity=200`, never `capacity=0`)
- [ ] Register assertions use `TEST_ASSERT_TRUE(val > 0)` not `TEST_ASSERT_EQUAL_UINT16`

### Questions to Ask:

1. "If I changed this operator, would a Unity `TEST_ASSERT_*` fail?"
2. "If I negated this condition, would a test fail?"
3. "If I removed this statement, would a Fake/Mock record the absence?"
4. "If I returned early here, would a test fail?"
5. "If I returned the wrong enum value, would a string assertion catch it?"

---

## Strengthening Weak Tests (Unity / C++ Patterns)

### Pattern: Add Boundary Value Tests

```cpp
// Original weak test
void test_validate_capacity() {
    cfg.capacity = 200;
    TEST_ASSERT_TRUE(role.validate());
}

// Strengthened with boundary values
void test_validate_rejects_zero_capacity() {
    cfg.capacity = 0;
    TEST_ASSERT_FALSE(role.validate());
}

void test_validate_accepts_minimum_nonzero_capacity() {
    cfg.capacity = 1;
    TEST_ASSERT_TRUE(role.validate());
}
```

### Pattern: Test Both Branches of Conditions

```cpp
// Original weak test — only tests one branch
void test_validate_passes() {
    cfg.minCurrent = 1.0f; cfg.maxCurrent = 5.0f;
    TEST_ASSERT_TRUE(role.validate());
}

// Strengthened — tests each branch independently
void test_validate_rejects_zero_capacity() {
    cfg.capacity = 0;
    cfg.minCurrent = 1.0f; cfg.maxCurrent = 5.0f;  // range valid
    TEST_ASSERT_FALSE(role.validate());
}

void test_validate_rejects_inverted_current_range() {
    cfg.capacity = 200;
    cfg.minCurrent = 5.0f; cfg.maxCurrent = 1.0f;  // capacity valid
    TEST_ASSERT_FALSE(role.validate());
}
```

### Pattern: Avoid Identity Values

```cpp
// Weak — identity values hide operator mutations
void test_percent_at_midscale() {
    // min=0, max=1 → denominator=1, so / and * give same result
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calcPercent(0.5f, 0.0f, 1.0f));
}

// Strong — values that reveal operator differences
void test_percent_at_midscale() {
    // min=1, max=5, current=3 → (3-1)/(5-1)=0.5 vs (3-1)*(5-1)=8.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calcPercent(3.0f, 1.0f, 5.0f));
}
```

### Pattern: Verify Side Effects via Fakes

```cpp
// Weak — no verification of what was sent
void test_loop_runs() {
    role.loop();
    // No assertion — empty loop() also "runs"
}

// Strong — Fake records what the mock received
void test_loop_sends_correct_metric_value() {
    manager.sensor.reading.current = 3.0f;  // mid-scale
    role.loop();
    TEST_ASSERT_TRUE(nmea.sent);
    TEST_ASSERT_EQUAL_INT((int)MetricType::FluidLevel, (int)nmea.lastMetric.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, nmea.lastMetric.value);
}
```

### Pattern: Assert Exact Register Values (for service-layer tests)

```cpp
// Weak — passes for any non-zero value
TEST_ASSERT_TRUE(cal > 0);

// Strong — exact formula result from datasheet
// Cal = trunc(0.04096 / (currentLsb * shuntOhms))
TEST_ASSERT_EQUAL_UINT16(409, cal);

// Also verify exact bit layout when assembling 16-bit values from bytes
uint16_t val = (uint16_t)((w.data[0] << 8) | w.data[1]);
TEST_ASSERT_EQUAL_UINT16(expectedRegisterValue, val);
```

### Pattern: Verify JSON Round-Trip Exactly

```cpp
// Weak — only checks configure doesn't fail
TEST_ASSERT_TRUE(role.configureFromJson(doc));

// Strong — verifies full serialize/deserialize cycle
void test_config_round_trips_through_json() {
    JsonDocument in;
    in["type"] = "FluidLevel";
    in["fluidType"] = "FuelGasoline";
    in["capacity"] = 257;
    in["minCurrent"] = 1.0f;
    in["maxCurrent"] = 5.0f;

    TEST_ASSERT_TRUE(role.configureFromJson(in));

    JsonDocument out;
    role.getConfigJson(out);

    TEST_ASSERT_EQUAL_STRING("FluidLevel", out["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("FuelGasoline", out["fluidType"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(257, out["capacity"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, out["minCurrent"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, out["maxCurrent"].as<float>());
}
```

---

## Quick Reference

### Operators Most Likely to Have Surviving Mutants

1. `>=` vs `>` (boundary not tested — test with exact boundary value)
2. `&&` vs `||` (only tested when both true/false — test each condition alone)
3. `+` vs `-` (only tested with 0.0 — use non-zero values)
4. `*` vs `/` (only tested with min=0, max=1 — use realistic calibration ranges)
5. `==` vs `!=` in enum guards (only tested with valid input — test invalid too)

### Test Values That Kill Mutants

| Avoid | Use Instead |
|-------|-------------|
| `capacity = 0, 1` only | Also test `capacity = 0` (boundary) |
| `min=0.0, max=1.0` | Use `min=1.0, max=5.0` (real calibration values) |
| `current=0.5` with `min=0, max=1` | `current=3.0` with `min=1.0, max=5.0` |
| All conditions true/false together | One true, one false |
| `TEST_ASSERT_TRUE(val > 0)` | `TEST_ASSERT_EQUAL_UINT16(exact, val)` |
| Only valid enum strings | Also test unrecognized strings |

### Unity Assertion Strength

| Weak (survives mutants) | Strong (kills mutants) |
|-------------------------|------------------------|
| `TEST_ASSERT_TRUE(role.validate())` | Also test `TEST_ASSERT_FALSE` paths |
| `TEST_ASSERT_TRUE(nmea.sent)` | `TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, nmea.lastMetric.value)` |
| `TEST_ASSERT_NOT_NULL(ptr)` | `TEST_ASSERT_EQUAL_STRING("exact", ptr)` |
| `TEST_ASSERT_TRUE(found)` | `TEST_ASSERT_EQUAL_UINT16(409, cal)` |
