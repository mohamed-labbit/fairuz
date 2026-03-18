#ifndef VALUE_HPP
#define VALUE_HPP

#include "../array.hpp"
#include "../string.hpp"
#include "opcode.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// NaN-boxing layout (64-bit)
//
//  Doubles   : any bit pattern where bits 63-51 are NOT 0x7FF9 / 0xFFF8 tags
//              (all normal/denormal/inf/nan IEEE 754 doubles land here)
//
//  ┌──────────────────────────────────────────────────────────────────────┐
//  │ 63    51 50    48  47                                              0  │
//  ├──────────────────────────────────────────────────────────────────────┤
//  │  0x7FF8   000     payload=0                         NIL  (=0x001)  │
//  │  0x7FF8   000     payload=0                        BOOL  (2=F,3=T) │
//  │  0x7FF9   xxx     48-bit signed integer payload                    │
//  │  0xFFF8   000     48-bit pointer (ARM64 user VA ≤ 48 bits)         │
//  └──────────────────────────────────────────────────────────────────────┘
//
//  Discrimination (cheapest to most expensive, ordered for fast dispatch):
//    IS_NIL      :  v == NIL_VAL                        (one compare)
//    IS_BOOL     :  (v | 1) == TRUE_VAL                 (one OR + compare)
//    IS_INTEGER  :  (v >> 48) == INT_TAG16              (one shift + compare)
//    IS_OBJECT   :  (v >> 48) == OBJ_TAG16              (one shift + compare)
//    isDouble   :  everything else
//
//  Payload extraction:
//    bool    :  v & 1                                  (one AND)
//    integer :  sign-extend bits 0-47 from bit 47      (mask + conditional OR)
//    object  :  v & PTR_MASK                           (one AND)
//    double  :  memcpy 8 bytes                         (register reinterpret)
//
// ============================================================================

namespace mylang::runtime {

// ---------------------------------------------------------------------------
// Raw type alias + sentinel constants
// ---------------------------------------------------------------------------

using Value = uint64_t;

static constexpr Value NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
static constexpr Value NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);

// Bit 48 set on top of QNAN → integer tag (top 16 bits = 0x7FF9)
static constexpr Value TAG_INT = UINT64_C(0x7FF9000000000000);
// SIGN + QNAN in top 16 bits → object pointer tag (top 16 bits = 0xFFF8)
static constexpr Value TAG_OBJ = UINT64_C(0xFFF8000000000000);
// Mask to extract the 48-bit pointer or integer payload
static constexpr Value PAYLOAD_MASK = UINT64_C(0x0000FFFFFFFFFFFF);

// Precomputed top-16-bit tags for the single-shift discriminator
static constexpr uint64_t INT_TAG16 = UINT64_C(0x7FF9);
static constexpr uint64_t OBJ_TAG16 = UINT64_C(0xFFF8);

// Nil / bool sentinels occupy the QNAN space with zero in bits 48-63
// (top 16 = 0x7FF8, same as canonical qNaN — they are disambiguated by the
//  payload: doubles never have tag=0x7FF8 AND a tiny nonzero payload because
//  the qNaN mantissa bits 51-62 must be nonzero for a quiet NaN)
static constexpr Value NIL_VAL = UINT64_C(0x7FF8000000000001);
static constexpr Value FALSE_VAL = UINT64_C(0x7FF8000000000002);
static constexpr Value TRUE_VAL = UINT64_C(0x7FF8000000000003);

// ---------------------------------------------------------------------------
// Object type system
// ---------------------------------------------------------------------------

enum class ObjType : uint8_t {
    STRING,
    LIST,
    FUNCTION,
    CLOSURE,
    NATIVE,
    UPVALUE
};

struct ObjHeader {
    ObjType type { ObjType::UPVALUE };
    bool isMarked { false };
    ObjHeader* next { nullptr };

    explicit ObjHeader(ObjType t) noexcept
        : type(t)
    {
    }
};

struct ObjString : ObjHeader {
    StringRef str;
    uint64_t hash;

    explicit ObjString(StringRef s)
        : ObjHeader(ObjType::STRING)
        , str(s)
        , hash(static_cast<uint64_t>(std::hash<StringRef> { }(s)))
    {
    }
};

struct ObjList : ObjHeader {
    Array<Value> elements;

    ObjList()
        : ObjHeader(ObjType::LIST)
    {
    }

    void reserve(uint32_t cap) { elements.reserve(cap); }
    uint32_t size() const { return elements.size(); }
};

// Forward declaration — Chunk is defined in opcode.hpp
struct ObjFunction : ObjHeader {
    unsigned int arity { 0 };
    unsigned int upvalueCount { 0 };
    Chunk* chunk { nullptr };
    ObjString* name { nullptr };

    explicit ObjFunction(Chunk* ch = nullptr)
        : ObjHeader(ObjType::FUNCTION)
        , chunk(ch)
    {
    }
};

struct ObjUpvalue : ObjHeader {
    Value* location { nullptr };
    Value closed;
    ObjUpvalue* nextOpen { nullptr };

    explicit ObjUpvalue(Value* slot) noexcept
        : ObjHeader(ObjType::UPVALUE)
        , location(slot)
    {
    }
};

struct ObjClosure : ObjHeader {
    ObjFunction* function { nullptr };
    Array<ObjUpvalue*> upValues;

    explicit ObjClosure(ObjFunction* fn)
        : ObjHeader(ObjType::CLOSURE)
        , function(fn)
        , upValues(fn->upvalueCount, nullptr)
    {
    }
};

using NativeFn = Value (*)(int argc, Value* argv);

struct ObjNative : ObjHeader {
    NativeFn fn;
    ObjString* name { nullptr };
    int arity;

    ObjNative(NativeFn f, ObjString* n, int a)
        : ObjHeader(ObjType::NATIVE)
        , fn(f)
        , name(n)
        , arity(a)
    {
    }
};

// ---------------------------------------------------------------------------
// Heap allocation helpers (thin wrappers around the arena allocator)
// ---------------------------------------------------------------------------

static inline ObjString* makeObjectString(StringRef s) { return getRuntimeAllocator().allocateObject<ObjString>(s); }
static inline ObjList* makeObjectList() { return getRuntimeAllocator().allocateObject<ObjList>(); }
static inline ObjFunction* makeObjectFunction(Chunk* ch = nullptr) { return getRuntimeAllocator().allocateObject<ObjFunction>(ch); }
static inline ObjUpvalue* makeObjectUpvalue(Value* slot) { return getRuntimeAllocator().allocateObject<ObjUpvalue>(slot); }
static inline ObjClosure* makeObjectClosure(ObjFunction* fn) { return getRuntimeAllocator().allocateObject<ObjClosure>(fn); }
static inline ObjNative* makeObjectNative(NativeFn f, ObjString* n, int a) { return getRuntimeAllocator().allocateObject<ObjNative>(f, n, a); }

// ---------------------------------------------------------------------------
// Value constructors  (Value::xxx() naming removed — now free functions)
// ---------------------------------------------------------------------------

[[nodiscard]] inline Value makeNil() noexcept { return NIL_VAL; }
[[nodiscard]] inline Value makeBool(bool b) noexcept { return b ? TRUE_VAL : FALSE_VAL; }

// Encodes a 48-bit signed integer. Values outside [-2^47, 2^47) are silently
// truncated to 48 bits — the same behaviour as Lua 5.4's integer NaN-boxing.
[[nodiscard]] inline Value makeInteger(int64_t v) noexcept
{
    // Mask to 48 bits, then OR in the integer tag.
    // Negative values: sign-extension bits above bit 47 are stripped by the mask;
    // asInteger() restores them during extraction.
    return (static_cast<Value>(v) & PAYLOAD_MASK) | TAG_INT;
}

// Stores an IEEE 754 double. Any bit pattern that is NOT a tagged NaN is a
// valid double; inf, sNaN, and qNaN all round-trip correctly.
[[nodiscard]] inline Value makeReal(double d) noexcept
{
    Value bits;
    __builtin_memcpy(&bits, &d, sizeof bits);
    return bits;
}

// Stores an object pointer. On ARM64 user-space, VA ≤ 48 bits is guaranteed
// for the current address space layout, so the top 16 bits of the pointer are
// always zero and the PAYLOAD_MASK round-trip is lossless.
// SAFETY: ptr must not be null — makeObject(nullptr) encodes as 0xFFF8000000000000
// which decodes back to nullptr, but IS_OBJECT() will still return true, so a
// subsequent type-check will deref a null ObjHeader. Use Value::nil() instead.
[[nodiscard]] inline Value makeObject(ObjHeader* ptr) noexcept
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(ptr) & PAYLOAD_MASK);
}

// ---------------------------------------------------------------------------
// Type predicates  — ordered from cheapest to hottest in the dispatch loop
// ---------------------------------------------------------------------------

#define IS_NIL(v) ((v) == NIL_VAL)
#define IS_BOOL(v) (((v) | 1) == TRUE_VAL)
#define IS_INTEGER(v) (((v) >> 48) == INT_TAG16)
#define IS_OBJECT(v) (((v) >> 48) == OBJ_TAG16)

// Everything that isn't one of the above is a double.
// The canonical qNaN (0x7FF8000000000000) satisfies: top16=0x7FF8 (not INT or OBJ),
// is not NIL/BOOL (payload=0), and falls through here. It is a valid double.
[[nodiscard]] inline bool isDouble(Value v) noexcept
{
    uint64_t top = v >> 48;
    return top != INT_TAG16 && top != OBJ_TAG16 && !IS_BOOL(v) && !IS_NIL(v);
}

// Compound predicates — each is two cheap checks.
[[nodiscard]] inline bool isNumber(Value v) noexcept { return IS_INTEGER(v) || isDouble(v); }

// Object subtype predicates — IS_OBJECT() is guaranteed true by callers that
// already checked; spelled out here for call sites that haven't.
[[nodiscard]] inline bool isString(Value v) noexcept
{
    return IS_OBJECT(v) && reinterpret_cast<ObjHeader*>(v & PAYLOAD_MASK)->type == ObjType::STRING;
}
[[nodiscard]] inline bool isList(Value v) noexcept
{
    return IS_OBJECT(v) && reinterpret_cast<ObjHeader*>(v & PAYLOAD_MASK)->type == ObjType::LIST;
}
[[nodiscard]] inline bool isFunction(Value v) noexcept
{
    return IS_OBJECT(v) && reinterpret_cast<ObjHeader*>(v & PAYLOAD_MASK)->type == ObjType::FUNCTION;
}
[[nodiscard]] inline bool isClosure(Value v) noexcept
{
    return IS_OBJECT(v) && reinterpret_cast<ObjHeader*>(v & PAYLOAD_MASK)->type == ObjType::CLOSURE;
}
[[nodiscard]] inline bool isNative(Value v) noexcept
{
    return IS_OBJECT(v) && reinterpret_cast<ObjHeader*>(v & PAYLOAD_MASK)->type == ObjType::NATIVE;
}

// ---------------------------------------------------------------------------
// Value extractors
// ---------------------------------------------------------------------------

// Extracts the boolean payload. Caller must have verified IS_BOOL(v).
[[nodiscard]] inline bool asBool(Value v) noexcept { return v & 1; }

// Extracts the 48-bit signed integer, sign-extended to int64_t.
// Caller must have verified IS_INTEGER(v).
[[nodiscard]] inline int64_t asInteger(Value v) noexcept
{
    // payload lives in bits 0-47
    int64_t payload = static_cast<int64_t>(v & PAYLOAD_MASK);
    // sign-extend: if bit 47 is set, fill bits 48-63 with ones
    //   arithmetic: payload |= ~PAYLOAD_MASK   (branchless on x86/ARM)
    //   equivalent: payload - ((payload & (INT64_C(1) << 47)) << 1)
    // The branch version is written explicitly so clang/GCC can fold it into
    // a single SBFX on ARM64.
    if (payload & (INT64_C(1) << 47))
        payload |= ~PAYLOAD_MASK;
    return payload;
}

// Identical to asInteger but skips the IS_INTEGER() guard — for use in the
// dispatch loop where the opcode guarantees the type.
[[nodiscard]] inline int64_t asIntegerUnchecked(Value v) noexcept
{
    int64_t payload = static_cast<int64_t>(v & PAYLOAD_MASK);
    if (payload & (INT64_C(1) << 47))
        payload |= ~PAYLOAD_MASK;
    return payload;
}

// Extracts the double. Caller must have verified isDouble(v).
[[nodiscard]] inline double asDouble(Value v) noexcept
{
    double d;
    __builtin_memcpy(&d, &v, sizeof d);
    return d;
}

// Converts any numeric Value to double (used for mixed int/float arithmetic).
// Caller must have verified isNumber(v).
[[nodiscard]] inline double asDoubleAny(Value v) noexcept
{
    if (IS_INTEGER(v)) [[unlikely]] // integers are rarer in float contexts
        return static_cast<double>(asIntegerUnchecked(v));
    double d;
    __builtin_memcpy(&d, &v, sizeof d);
    return d;
}

// Extracts the raw ObjHeader pointer. Caller must have verified IS_OBJECT(v).
[[nodiscard]] inline ObjHeader* asObject(Value v) noexcept
{
    return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(v & PAYLOAD_MASK));
}

// Typed object extractors — no additional type check; caller is responsible.
[[nodiscard]] inline ObjString* asString(Value v) noexcept { return static_cast<ObjString*>(asObject(v)); }
[[nodiscard]] inline ObjList* asList(Value v) noexcept { return static_cast<ObjList*>(asObject(v)); }
[[nodiscard]] inline ObjFunction* asFunction(Value v) noexcept { return static_cast<ObjFunction*>(asObject(v)); }
[[nodiscard]] inline ObjClosure* asClosure(Value v) noexcept { return static_cast<ObjClosure*>(asObject(v)); }
[[nodiscard]] inline ObjNative* asNative(Value v) noexcept { return static_cast<ObjNative*>(asObject(v)); }

// ---------------------------------------------------------------------------
// isTruthy  — the hot path for every conditional and JUMP_IF_FALSE
//
//  Falsy values: nil, false, integer 0, double ±0.0
//  Truthy values: true, any nonzero number, any object pointer
//
//  Ordering of checks is tuned for a language where integers and booleans
//  dominate conditionals.  The double ±0.0 path is marked unlikely.
// ---------------------------------------------------------------------------

[[nodiscard]] inline bool isTruthy(Value v) noexcept
{
    // Nil: only one specific bit pattern.
    if (__builtin_expect(v == NIL_VAL, 0))
        return false;

    // Bool: bit 0 of the low 2-bit payload is the truth value.
    // FALSE_VAL=...010 → bit0=0, TRUE_VAL=...011 → bit0=1.
    if (__builtin_expect((v | 1) == TRUE_VAL, 0))
        return v & 1;

    // Integer: payload in bits 0-47; truthy iff any payload bit is set.
    // This is the hottest branch in numeric loops.
    if (__builtin_expect((v >> 48) == INT_TAG16, 1))
        return (v & PAYLOAD_MASK) != 0;

    // Object pointer: always truthy.  OBJ_TAG16=0xFFF8.
    if (__builtin_expect((v >> 48) == OBJ_TAG16, 0))
        return true;

    // Double: left-shift 1 strips the IEEE sign bit, making ±0.0 both map
    // to 0.  Any nonzero double (including NaN, inf) has v<<1 != 0.
    // NaN is truthy here (Python/Ruby/JS behaviour varies; this matches Ruby).
    return __builtin_expect((v << 1) != 0, 1);
}

// ---------------------------------------------------------------------------
// IC / profiling support: type tag bitmask
// ---------------------------------------------------------------------------

enum class TypeTag : uint8_t {
    NONE = 0,
    NIL = 1 << 0,
    BOOL = 1 << 1,
    INT = 1 << 2,
    DOUBLE = 1 << 3,
    STRING = 1 << 4,
    LIST = 1 << 5,
    CLOSURE = 1 << 6,
    NATIVE = 1 << 7
};

[[nodiscard]] inline bool hasTag(TypeTag mask, TypeTag t) noexcept
{
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(t)) != 0;
}

[[nodiscard]] inline TypeTag operator|(TypeTag a, TypeTag b) noexcept
{
    return static_cast<TypeTag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline TypeTag& operator|=(TypeTag& a, TypeTag b) noexcept
{
    return a = a | b;
}

// Compute the TypeTag for a value — used only in IC profiling (cold path).
[[nodiscard]] inline TypeTag valueTypeTag(Value v) noexcept
{
    if (v == NIL_VAL)
        return TypeTag::NIL;
    if ((v | 1) == TRUE_VAL)
        return TypeTag::BOOL;
    if ((v >> 48) == INT_TAG16)
        return TypeTag::INT;

    if ((v >> 48) == OBJ_TAG16) {
        switch (asObject(v)->type) {
        case ObjType::STRING:
            return TypeTag::STRING;
        case ObjType::LIST:
            return TypeTag::LIST;
        case ObjType::CLOSURE:
            return TypeTag::CLOSURE;
        case ObjType::NATIVE:
            return TypeTag::NATIVE;
        default:
            return TypeTag::NONE;
        }
    }

    return TypeTag::DOUBLE;
}

} // namespace mylang::runtime

#endif // VALUE_HPP
