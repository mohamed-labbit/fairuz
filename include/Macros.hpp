//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines compiler and portability macros.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_MACROS_HPP
#define FAIRUZ_MACROS_HPP

#include <compare>
#include <cstdint>
#include <string>

namespace fairuz {

struct SourceLocation {
    uint32_t line { 0 };
    uint16_t column { 0 };
    uint16_t length { 0 };
    uint64_t offset { 0 }; // byte offset
}; // struct SourceLocation

struct VoidPtrHash {
    size_t operator()(void const* ptr) const noexcept
    {
        return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(ptr));
    }
}; // struct VoidPtrHash

struct VoidPtrEqual {
    bool operator()(void const* a, void const* b) const noexcept { return a == b; }
}; // struct VoidPtrEqual

namespace Color {

std::string const RESET = "\033[0m";
std::string const BOLD = "\033[1m";
std::string const RED = "\033[31m";
std::string const GREEN = "\033[32m";
std::string const YELLOW = "\033[33m";
std::string const BLUE = "\033[34m";
std::string const MAGENTA = "\033[35m";
std::string const CYAN = "\033[36m";
std::string const GRAY = "\033[90m";

} // namespace Color

#define SSO_SIZE 22
#define TABWIDTH 8
#define MAX_ALLOWED_INDENT 100
#define DEFAULT_CAPACITY 4096
#define DEFAULT_BLOCK_SIZE 1024
#define MAX_BLOCK_SIZE 4294967296
#define DEFAULT_STRING_CAPACITY 100 FAIRUZ_NODISCARD
#define FAIRUZ_NODISCARD [[nodiscard]]
#define MAIN_PROGRAM "__main__"

} // namespace fairuz

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f64 = double;

// Semantic identities, not integer substitutes. Arithmetic belongs to the
// owning subsystem; unwrap explicitly at storage and bytecode boundaries.
template<typename T, typename Tag> struct StrongAlias {
    T value { };

    constexpr StrongAlias() noexcept = default;

    explicit constexpr StrongAlias(T v) noexcept
        : value(v)
    {
    }

    constexpr auto operator<=>(StrongAlias const&) const noexcept = default;
};

using RegType = StrongAlias<u8, struct RegTypeTag>;
// Pool indexes are local to a chunk, but not interchangeable between pools.
using ConstantIndex = StrongAlias<u16, struct ConstantIndexTag>;
using BigIntIndex = StrongAlias<u16, struct BigIntIndexTag>;
using FunctionIndex = StrongAlias<u16, struct FunctionIndexTag>;
using ClassIndex = StrongAlias<u16, struct ClassIndexTag>;
using PCType = StrongAlias<u32, struct PCTypeTag>;
using CacheSlot = StrongAlias<u8, struct CacheSlotTag>;
using MethodSlot = StrongAlias<u8, struct MethodSlotTag>;

// Encoded instruction words have no single operand interpretation.
using InstrType = u32;

#if defined(__GNUC__) || defined(__clang__)
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)
#    define LIKELY(x) __builtin_expect(!!(x), 1)
#else
#    define UNLIKELY(x) (!!(x))
#    define LIKELY(x) (!!(x))
#endif

#if defined(__x86_64__) || defined(_M_X64)
#    define FA_USE_NANBOX 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#    define FA_USE_NANBOX 1
#else
#    define FA_USE_NANBOX 0
#endif

#endif // FAIRUZ_MACROS_HPP
