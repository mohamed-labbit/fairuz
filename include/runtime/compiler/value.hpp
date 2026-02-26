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

inline bool nanbox_is_double(uint64_t v)
{
    return (v & NANBOX_QNAN) != NANBOX_QNAN || v == CANONICAL_NAN;
}

inline bool nanbox_is_obj(uint64_t v)
{
    return (v & (NANBOX_SIGN_BIT | NANBOX_QNAN)) == (NANBOX_SIGN_BIT | NANBOX_QNAN);
}

inline bool nanbox_is_int(uint64_t v)
{
    return (v & (NANBOX_QNAN | TAG_INT)) == (NANBOX_QNAN | TAG_INT) && !nanbox_is_obj(v);
}

inline bool nanbox_is_bool(uint64_t v)
{
    return (v & (NANBOX_QNAN | TAG_BOOL | TAG_INT)) == (NANBOX_QNAN | TAG_BOOL);
}

inline bool nanbox_is_nil(uint64_t v)
{
    return v == NIL_VAL;
}

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

    static Value nil()
    {
        return Value { NIL_VAL };
    }

    static Value boolean(bool b)
    {
        return Value {
            b ? TRUE_VAL : FALSE_VAL
        };
    }

    static Value integer(int64_t v)
    {
        // Store the lower 48 bits; sign-extend on read.
        // For values outside ±2^47 we fall back to double.
        if (v >= -(INT64_C(1) << 47) && v < (INT64_C(1) << 47)) {
            uint64_t payload = static_cast<uint64_t>(v) & UINT64_C(0xFFFFFFFFFFFF);

            return Value {
                NANBOX_QNAN | TAG_INT | (payload << 3)
            };
        }

        return Value::real(static_cast<double>(v));
    }

    static Value real(double d)
    {
        uint64_t bits;
        ::memcpy(&bits, &d, 8);

        // If NaN → force canonical
        if ((bits & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (bits & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)
            bits = CANONICAL_NAN;

        return Value { bits };
    }

    static Value object(ObjHeader* ptr)
    {
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

        return Value {
            NANBOX_SIGN_BIT | NANBOX_QNAN | (static_cast<uint64_t>(p) & UINT64_C(0xFFFFFFFFFFFF))
        };
    }

    // ---- type queries ----
    bool isNil() const
    {
        return nanbox_is_nil(raw);
    }

    bool isBool() const
    {
        return nanbox_is_bool(raw);
    }

    bool isInt() const
    {
        return nanbox_is_int(raw);
    }

    bool isDouble() const
    {
        return nanbox_is_double(raw);
    }

    bool isNumber() const
    {
        return isInt() || isDouble();
    }

    bool isObj() const
    {
        return nanbox_is_obj(raw);
    }

    // ---- extractors ----
    bool asBool() const
    {
        assert(isBool());
        return (raw >> 2) & 1;
    }

    int64_t asInt() const
    {
        assert(isInt());
        // Extract 48-bit payload from bits 3..50, sign-extend.
        int64_t payload = static_cast<int64_t>((raw >> 3) & UINT64_C(0xFFFFFFFFFFFF));
        // Sign-extend from bit 47
        if (payload & (INT64_C(1) << 47))
            payload |= ~UINT64_C(0xFFFFFFFFFFFF);

        return payload;
    }

    double asDouble() const
    {
        assert(isDouble());
        double d;
        ::memcpy(&d, &raw, 8);
        return d;
    }

    double as_number_double() const
    {
        if (isInt())
            return static_cast<double>(asInt());

        return asDouble();
    }

    ObjHeader* asObj() const
    {
        assert(isObj());
        return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(raw & UINT64_C(0xFFFFFFFFFFFF)));
    }

    // ---- truthiness (falsy: nil, false, 0, 0.0) ----
    bool isTruthy() const
    {
        if (isNil())
            return false;

        if (isBool())
            return asBool();

        if (isInt())
            return asInt() != 0;

        if (isDouble())
            return asDouble() != 0.0;

        return true; // objects are truthy
    }

    bool operator==(Value other) const
    {
        return raw == other.raw;
    }

    bool operator!=(Value other) const
    {
        return raw != other.raw;
    }

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
        , hash(0)
    {
        // compute_hash
    }

    static uint32_t compute_hash(StringRef s);
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

    ObjFunction();
    ~ObjFunction();
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

// Inline type helpers (depend on ObjHeader::type)
inline bool Value::isString() const
{
    return isObj() && asObj()->type == ObjType::STRING;
}

inline bool Value::isList() const
{
    return isObj() && asObj()->type == ObjType::LIST;
}

inline bool Value::isFunction() const
{
    return isObj() && asObj()->type == ObjType::FUNCTION;
}
inline bool Value::isClosure() const
{
    return isObj() && asObj()->type == ObjType::CLOSURE;
}

inline bool Value::isNative() const
{
    return isObj() && asObj()->type == ObjType::NATIVE;
}

inline ObjString* Value::asString() const
{
    return static_cast<ObjString*>(asObj());
}

inline ObjList* Value::asList() const
{
    return static_cast<ObjList*>(asObj());
}

inline ObjFunction* Value::asFunction() const
{
    return static_cast<ObjFunction*>(asObj());
}

inline ObjClosure* Value::asClosure() const
{
    return static_cast<ObjClosure*>(asObj());
}

inline ObjNative* Value::asNative() const
{
    return static_cast<ObjNative*>(asObj());
}

// Pretty-printer (defined in value.cpp)
inline std::ostream& operator<<(std::ostream& os, Value v)
{
    using namespace mylang::runtime;

    if (v.isNil()) {
        os << "nil";
        return os;
    }

    if (v.isBool()) {
        os << (v.asBool() ? "true" : "false");
        return os;
    }

    if (v.isInt()) {
        os << v.asInt();
        return os;
    }

    if (v.isDouble()) {
        os << v.asDouble();
        return os;
    }

    if (v.isObj()) {
        ObjHeader* obj = v.asObj();

        switch (obj->type) {
        case ObjType::STRING: {
            ObjString* s = static_cast<ObjString*>(obj);
            os << s->chars;
            break;
        }

        case ObjType::LIST: {
            ObjList* list = static_cast<ObjList*>(obj);
            os << "[";
            for (size_t i = 0; i < list->elements.size(); ++i) {
                os << list->elements[i];
                if (i + 1 < list->elements.size())
                    os << ", ";
            }
            os << "]";
            break;
        }

        case ObjType::FUNCTION: {
            ObjFunction* fn = static_cast<ObjFunction*>(obj);
            if (fn->name)
                os << "<fn " << fn->name->chars << ">";
            else
                os << "<fn anonymous>";
            break;
        }

        case ObjType::CLOSURE: {
            ObjClosure* closure = static_cast<ObjClosure*>(obj);
            if (closure->function->name)
                os << "<closure " << closure->function->name->chars << ">";
            else
                os << "<closure anonymous>";
            break;
        }

        case ObjType::NATIVE: {
            ObjNative* native = static_cast<ObjNative*>(obj);
            os << "<native fn " << native->name->chars << ">";
            break;
        }

        case ObjType::UPVALUE:
            os << "<upvalue>";
            break;

        default:
            os << "<unknown object>";
            break;
        }

        return os;
    }

    // Should never happen
    os << "<invalid>";
    return os;
}

} // namespace runtime
} // namespace mylang
