#pragma once
#include <cstring>

// ============================================================================
// MACRO GENERATOR - Reuse for any enum type and type/string conversions
// ============================================================================

#define GENERATE_ENUM(EnumName, ListMacro) \
    enum class EnumName { ListMacro(GENERATE_ENUM_VALUE) };

#define GENERATE_ENUM_VALUE(name) name,

// Helper macro to handle the EnumName::name syntax
#define GENERATE_TO_STRING_CASE(EnumName, name) \
    case EnumName::name:                        \
        return #name;

#define GENERATE_TO_STRING(EnumName, ListMacro)                       \
    inline const char* EnumName##ToString(EnumName value) {           \
        switch (value) { ListMacro(GENERATE_TO_STRING_CASE_WRAPPER) } \
        return "Unavailable";                                         \
    }

// This wrapper is needed to pass EnumName through
#define GENERATE_TO_STRING_CASE_WRAPPER(name) \
    GENERATE_TO_STRING_CASE(CURRENT_ENUM_NAME, name)

// Similar approach for FromString
#define GENERATE_FROM_STRING_CHECK(EnumName, name) \
    if (strcmp(str, #name) == 0) return EnumName::name;

#define GENERATE_FROM_STRING(EnumName, ListMacro)                             \
    inline EnumName EnumName##FromString(const char* str) {                   \
        ListMacro(                                                            \
            GENERATE_FROM_STRING_CHECK_WRAPPER) return EnumName::Unavailable; \
    }

#define GENERATE_FROM_STRING_CHECK_WRAPPER(name) \
    GENERATE_FROM_STRING_CHECK(CURRENT_ENUM_NAME, name)

// FluidType
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

#define CURRENT_ENUM_NAME FluidType
GENERATE_ENUM(FluidType, FLUID_TYPE_LIST)
GENERATE_TO_STRING(FluidType, FLUID_TYPE_LIST)
GENERATE_FROM_STRING(FluidType, FLUID_TYPE_LIST)
#undef CURRENT_ENUM_NAME
