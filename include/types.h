#pragma once
#include <string>

// --- Single source of truth ---
#define FLUID_TYPE_LIST(X) \
    X(Fuel)                \
    X(Water)               \
    X(GrayWater)           \
    X(LiveWell)            \
    X(Oil)                 \
    X(BlackWater)          \
    X(FuelGasoline)        \
    X(Error)               \
    X(Unavailable)

// --- Enum definition ---
enum class FluidType {
#define X(name) name,
    FLUID_TYPE_LIST(X)
#undef X
};
