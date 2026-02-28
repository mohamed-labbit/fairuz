#pragma once

#include "../../types/string.hpp"
#include "forward.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace mylang {
namespace runtime {

// ---------------------------------------------------------------------------
// NaN-boxing layout (64-bit)
//
// IEEE 754 double: any value with bits 51..62 all set is a NaN.
// We exploit the 51 payload bits + the sign bit to encode non-double types.
//
//  63       51       48                                0
//  ┌────────┬─────────┬──────────────────────────────────┐
//  │ sign   │ 111...1 │  payload (48 bits)               │
//  └────────┴─────────┴──────────────────────────────────┘
//
//  sign=0, tag=0    → quiet NaN (not used by us)
//  NANBOX_TAG_INT   → int48  (sign-extended to int64 at use site)
//  NANBOX_TAG_BOOL  → 0=false, 1=true
//  NANBOX_TAG_NIL   → singleton
//  NANBOX_TAG_OBJ   → 48-bit heap pointer (enough for any real process)
//
// Pure doubles (including ±inf, but NOT NaN) are stored as-is.
// We reserve the "canonical NaN" (all payload bits 0) as a sentinel and
// never produce it; user NaN values are normalised to a fixed quiet NaN.
// ---------------------------------------------------------------------------

// Tag bits sit in bits 48..50 of the NaN payload.
static constexpr uint64_t NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
static constexpr uint64_t NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);
// canonical NaN value
static constexpr uint64_t CANONICAL_NAN = UINT64_C(0x7FF8000000000000);
// Tags encoded in bits 49..48 when the QNAN mask is set
static constexpr uint64_t TAG_NIL = UINT64_C(1);  // 01
static constexpr uint64_t TAG_BOOL = UINT64_C(2); // 10
static constexpr uint64_t TAG_INT = UINT64_C(3);  // 11
// Objects: SIGN_BIT | QNAN, payload = pointer (48-bit)
// (distinguishable because sign bit is set and tag bits are 00)
static constexpr uint64_t NIL_VAL = NANBOX_QNAN | TAG_NIL;
static constexpr uint64_t FALSE_VAL = NANBOX_QNAN | TAG_BOOL | 0;
static constexpr uint64_t TRUE_VAL = NANBOX_QNAN | TAG_BOOL | (UINT64_C(1) << 2);

// Value — a thin wrapper around uint64_t.  Passed by value everywhere.
class Value {
public:
    uint64_t raw;

    // ---- constructors ----
    Value()
        : raw(NIL_VAL)
    {
    }

    explicit Value(uint64_t r)
        : raw(r)
    {
    }

    static Value nil();
    static Value boolean(bool b);
    static Value integer(int64_t v);
    static Value real(double d);
    static Value object(ObjHeader* ptr);

    // ---- type queries ----
    bool isNil() const;
    bool isBool() const;
    bool isInt() const;
    bool isDouble() const;
    bool isObj() const;
    bool isNumber() const;

    // ---- extractors ----
    bool asBool() const;
    int64_t asInt() const;
    double asDouble() const;
    double asNumberDouble() const;
    ObjHeader* asObj() const;

    // ---- truthiness (falsy: nil, false, 0, 0.0) ----
    bool isTruthy() const;

    bool operator==(Value other) const { return raw == other.raw; }
    bool operator!=(Value other) const { return raw != other.raw; }

    // Defined in value.cpp
    ObjString* asString() const;
    ObjList* asList() const;
    ObjFunction* asFunction() const;
    ObjClosure* asClosure() const;
    ObjNative* asNative() const;

    bool isString() const;
    bool isList() const;
    bool isFunction() const;
    bool isClosure() const;
    bool isNative() const;
};

// Heap object header — every GC-managed object starts with this.
enum class ObjType : uint8_t {
    STRING,
    LIST,
    FUNCTION,
    CLOSURE,
    NATIVE,
    UPVALUE,
};

struct ObjHeader {
    ObjType type { ObjType::UPVALUE };
    bool is_marked { false };    // GC mark bit
    ObjHeader* next { nullptr }; // intrusive GC list

    ObjHeader() = default;

    explicit ObjHeader(ObjType t)
        : type(t)
        , is_marked(false)
        , next(nullptr)
    {
    }
};

// Heap object types

struct ObjString : ObjHeader {
    StringRef chars;
    uint32_t hash; // cached FNV-1a

    explicit ObjString(StringRef s)
        : ObjHeader(ObjType::STRING)
        , chars(s)
    {
        hash = compute_hash(chars);
    }

    static uint32_t compute_hash(StringRef s)
    {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < s.len();) {
            h ^= s[i++];
            h *= 16777619u;
        }
        return h;
    }
};

struct ObjList : ObjHeader {
    std::vector<Value> elements;

    ObjList()
        : ObjHeader(ObjType::LIST)
    {
    }
};

// Forward declaration — Chunk defined in chunk.hpp
struct Chunk;

struct ObjFunction : ObjHeader {
    int arity; // number of parameters
    int upvalue_count;

    Chunk* chunk;    // owned — non-null after compilation
    ObjString* name; // may be null for anonymous

    ObjFunction()
        : ObjHeader(ObjType::FUNCTION)
        , arity(0)
        , upvalue_count(0)
        , chunk(nullptr)
        , name(nullptr)
    {
    }

    ~ObjFunction()
    {
        // chunk is owned by whoever created it (Compiler or test).  Don't delete here.
    }
};

struct ObjUpvalue : ObjHeader {
    Value* location;       // points into the stack while open
    Value closed;          // holds the value after close
    ObjUpvalue* next_open; // intrusive list of open upvalues

    explicit ObjUpvalue(Value* slot)
        : ObjHeader(ObjType::UPVALUE)
        , location(slot)
        , next_open(nullptr)
    {
    }
};

struct ObjClosure : ObjHeader {
    ObjFunction* function;
    std::vector<ObjUpvalue*> upvalues;

    explicit ObjClosure(ObjFunction* fn)
        : ObjHeader(ObjType::CLOSURE)
        , function(fn)
    {
        upvalues.resize(fn->upvalue_count, nullptr);
    }
};

using NativeFn = Value (*)(int argc, Value* argv);

struct ObjNative : ObjHeader {
    NativeFn fn;
    ObjString* name;
    int arity; // -1 = variadic

    ObjNative(NativeFn f, ObjString* n, int a)
        : ObjHeader(ObjType::NATIVE)
        , fn(f)
        , name(n)
        , arity(a)
    {
    }
};

} // namespace runtime
} // namespace mylang
