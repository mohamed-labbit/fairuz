#ifndef FA_VALUE_HPP
#define FA_VALUE_HPP

#include "fmacros.hpp"
#include "fobj_header.hpp"

#include <cassert>
#include <cstdlib>

#if FA_USE_NANBOX

#    include "fstring.hpp"
#    include "ftable.hpp"

#    include <cstdint>

#endif

namespace fairuz::runtime {

// forward
struct Fa_ObjString;
struct Fa_ObjList;
struct Fa_ObjDict;
struct Fa_ObjFunction;
struct Fa_ObjNative;
struct Fa_ObjClass;
struct Fa_ObjInstance;
struct Fa_ObjFileHandle;

#if FA_USE_NANBOX

class Fa_VM; // forward

struct Fa_ValueHash {
    size_t operator()(u64 const& v) const noexcept { return v; }
};

struct Fa_ValueEqual {
    bool operator()(u64 const& lhs, u64 const& rhs) const noexcept { return lhs == rhs; }
};

using Fa_Value = u64;
using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);

/// NOTE: exclude any added tag from Fa_is_double macro
static constexpr Fa_Value NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
static constexpr Fa_Value NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);
static constexpr Fa_Value TAG_INT = UINT64_C(0x7FF9000000000000);
static constexpr Fa_Value TAG_OBJ = UINT64_C(0xFFF8000000000000);
static constexpr Fa_Value PAYLOAD_MASK = UINT64_C(0x0000FFFFFFFFFFFF);
static constexpr Fa_Value NIL_VAL = UINT64_C(0x7FF8000000000001);
static constexpr Fa_Value FALSE_VAL = UINT64_C(0x7FF8000000000002);
static constexpr Fa_Value TRUE_VAL = UINT64_C(0x7FF8000000000003);
static constexpr u64 INT_TAG16 = UINT64_C(0x7FF9);
static constexpr u64 OBJ_TAG16 = UINT64_C(0xFFF8);

// runtime values
static inline Fa_Value Fa_make_nil()
{
    return NIL_VAL;
}
static inline Fa_Value Fa_make_obj(Fa_ObjHeader const* p)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_make_real(f64 const d)
{
    Fa_Value bits;
    ::memcpy(&bits, &d, sizeof(bits));
    return bits;
}
static inline Fa_Value Fa_make_bool(bool const b)
{
    return b ? TRUE_VAL : FALSE_VAL;
}
static inline Fa_Value Fa_make_int(i64 const v)
{
    return (static_cast<Fa_Value>(v) & PAYLOAD_MASK) | TAG_INT;
}

static inline Fa_Value Fa_from_string(Fa_ObjString const* s)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(s) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_list(Fa_ObjList const* l)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(l) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_dict(Fa_ObjDict const* d)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(d) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_func(Fa_ObjFunction const* f)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(f) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_native(Fa_ObjNative const* n)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(n) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_class(Fa_ObjClass const* c)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(c) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_instance(Fa_ObjInstance const* i)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(i) & PAYLOAD_MASK);
}
static inline Fa_Value Fa_from_file_handle(Fa_ObjFileHandle const* p)
{
    return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK);
}

/* ------- Truth macros  ------- */

static inline bool Fa_is_nil(Fa_Value const v) { return v == NIL_VAL; }
static inline bool Fa_is_bool(Fa_Value const v) { return (v | 1) == TRUE_VAL; }
static inline bool Fa_is_int(Fa_Value const v) { return (v >> 48) == INT_TAG16; }
static inline bool Fa_is_obj(Fa_Value const v) { return (v >> 48) == OBJ_TAG16; }
static inline bool Fa_is_double(Fa_Value const v)
{
    u64 top = v >> 48;
    return top != INT_TAG16 && top != OBJ_TAG16 && !((v | 1) == TRUE_VAL) && !(v == NIL_VAL);
}
static inline bool Fa_is_number(Fa_Value const v)
{
    u64 top = v >> 48;
    return (top == INT_TAG16) || (top != OBJ_TAG16 && !((v | 1) == TRUE_VAL) && !(v == NIL_VAL));
}

static inline bool Fa_is_string(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::STRING);
}
static inline bool Fa_is_list(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::LIST);
}
static inline bool Fa_is_dict(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::DICT);
}
static inline bool Fa_is_function(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::FUNCTION);
}
static inline bool Fa_is_native(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::NATIVE);
}
static inline bool Fa_is_class(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::CLASS);
}
static inline bool Fa_is_instance(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::INSTANCE);
}
static inline bool Fa_is_file_handle(Fa_Value const v)
{
    return (Fa_is_obj(v) && reinterpret_cast<Fa_ObjHeader*>((v)&PAYLOAD_MASK)->type == Fa_ObjType::FILE_HANDLE);
}

static inline bool Fa_is_truthy(Fa_Value const v)
{
    if (Fa_is_nil(v))
        return false;
    else if (Fa_is_bool(v))
        return v & 1;
    else if (Fa_is_int(v))
        return (v & PAYLOAD_MASK) != 0;
    else if (Fa_is_obj(v))
        return true;
    return (v << 1) != 0;
}

/* -------- Casting macros -------- */

static inline bool Fa_as_bool(Fa_Value const v)
{
    return v & 1;
}
static inline i64 Fa_as_int(Fa_Value const v)
{
    i64 payload = static_cast<i64>(v & PAYLOAD_MASK);
    if (payload & (INT64_C(1) << 47))
        return payload | ~PAYLOAD_MASK;
    return payload;
}
static inline f64 Fa_as_double(Fa_Value const v)
{
    f64 d;
    ::memcpy(&d, &v, sizeof(d));
    return d;
}
static inline f64 Fa_as_double_any(Fa_Value v)
{
    return Fa_is_int(v) ? static_cast<f64>(Fa_as_int(v)) : Fa_as_double(v);
}
static inline Fa_ObjHeader* Fa_as_obj(Fa_Value const v)
{
    return reinterpret_cast<Fa_ObjHeader*>(static_cast<uintptr_t>(v & PAYLOAD_MASK));
}

enum class Fa_TypeTag : u16 {
    NONE = 0,
    NIL = 1 << 0,
    BOOL = 1 << 1,
    INT = 1 << 2,
    DOUBLE = 1 << 3,
    STRING = 1 << 4,
    LIST = 1 << 5,
    CLOSURE = 1 << 6,
    FUNCTION = 1 << 7,
    NATIVE = 1 << 8,
    CLASS = 1 << 9,
    INSTANCE = 1 << 10,
    DICT = 1 << 11,
    FILE_HANDLE = 1 << 12,
}; // enum Fa_TypeTag

[[nodiscard]] inline bool has_tag(Fa_TypeTag mask, Fa_TypeTag t) noexcept
{
    return (static_cast<u16>(mask) & static_cast<u16>(t)) != 0;
}

[[nodiscard]] inline Fa_TypeTag operator|(Fa_TypeTag a, Fa_TypeTag b) noexcept
{
    return static_cast<Fa_TypeTag>(static_cast<u16>(a) | static_cast<u16>(b));
}

inline Fa_TypeTag& operator|=(Fa_TypeTag& a, Fa_TypeTag b) noexcept { return a = a | b; }

[[nodiscard]] inline Fa_TypeTag value_type_tag(Fa_Value v) noexcept
{
    if (Fa_is_nil(v))
        return Fa_TypeTag::NIL;
    if (Fa_is_bool(v))
        return Fa_TypeTag::BOOL;
    if (Fa_is_int(v))
        return Fa_TypeTag::INT;
    if (Fa_is_double(v))
        return Fa_TypeTag::DOUBLE;

    if (Fa_is_obj(v)) {
        switch (Fa_as_obj(v)->type) {
        case Fa_ObjType::STRING: return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST: return Fa_TypeTag::LIST;
        case Fa_ObjType::DICT: return Fa_TypeTag::DICT;
        case Fa_ObjType::FUNCTION: return Fa_TypeTag::FUNCTION;
        case Fa_ObjType::NATIVE: return Fa_TypeTag::NATIVE;
        case Fa_ObjType::CLASS: return Fa_TypeTag::CLASS;
        case Fa_ObjType::INSTANCE: return Fa_TypeTag::INSTANCE;
        case Fa_ObjType::FILE_HANDLE: return Fa_TypeTag::FILE_HANDLE;
        default: return Fa_TypeTag::NONE;
        }
    }

    return Fa_TypeTag::NONE;
}

#else

enum class Fa_TypeTag : u16 {
    NONE,
    NIL,
    BOOL,
    INT,
    DOUBLE,
    // object tags
    STRING,
    LIST,
    CLOSURE,
    FUNCTION,
    NATIVE,
    CLASS,
    INSTANCE,
    DICT,
    FILE_HANDLE,
}; // enum Fa_TypeTag

class Fa_Value {
private:
    Fa_TypeTag m_type;

    union {
        bool b;
        i64 i;
        f64 f;
        Fa_ObjHeader* o;
    } as;

public:
    Fa_TypeTag type_tag() const { return m_type; }

    static Fa_Value nil()
    {
        Fa_Value v;
        v.m_type = Fa_TypeTag::NIL;
        return v;
    }
    static Fa_Value from_bool(bool bval)
    {
        Fa_Value v;
        v.m_type = Fa_TypeTag::BOOL;
        v.as.b = bval;
        return v;
    }
    static Fa_Value from_int(i64 ival)
    {
        Fa_Value v;
        v.m_type = Fa_TypeTag::INT;
        v.as.i = ival;
        return v;
    }
    static Fa_Value from_double(f64 fval)
    {
        Fa_Value v;
        v.m_type = Fa_TypeTag::DOUBLE;
        v.as.f = fval;
        return v;
    }
    static Fa_Value from_object(Fa_ObjHeader* oval)
    {
        assert(oval != nullptr);
        Fa_Value v;
        switch (oval->type) {
        case Fa_ObjType::CLASS: v.m_type = Fa_TypeTag::CLASS; break;
        case Fa_ObjType::DICT: v.m_type = Fa_TypeTag::DICT; break;
        case Fa_ObjType::FUNCTION: v.m_type = Fa_TypeTag::FUNCTION; break;
        case Fa_ObjType::INSTANCE: v.m_type = Fa_TypeTag::INSTANCE; break;
        case Fa_ObjType::LIST: v.m_type = Fa_TypeTag::LIST; break;
        case Fa_ObjType::NATIVE: v.m_type = Fa_TypeTag::NATIVE; break;
        case Fa_ObjType::STRING: v.m_type = Fa_TypeTag::STRING; break;
        case Fa_ObjType::FILE_HANDLE: v.m_type = Fa_TypeTag::FILE_HANDLE; break;
        default: v.m_type = Fa_TypeTag::NONE;
        }
        v.as.o = oval;
        return v;
    }

    bool is_nil() const { return m_type == Fa_TypeTag::NIL; }
    bool is_bool() const { return m_type == Fa_TypeTag::BOOL; }
    bool is_int() const { return m_type == Fa_TypeTag::INT; }
    bool is_double() const { return m_type == Fa_TypeTag::DOUBLE; }
    bool is_object() const { return m_type >= Fa_TypeTag::STRING && m_type <= Fa_TypeTag::FILE_HANDLE; }

    bool as_bool() const { return as.b; }
    i64 as_int() const { return as.i; }
    f64 as_double() const { return as.f; }
    Fa_ObjHeader* as_object() const { return as.o; }

    bool operator==(Fa_Value const& other) const
    {
        if (m_type != other.m_type)
            return false;
        switch (m_type) {
        case Fa_TypeTag::NONE: return false;
        case Fa_TypeTag::NIL: return true;
        case Fa_TypeTag::BOOL: return as_bool() == other.as_bool();
        case Fa_TypeTag::INT: return as_int() == other.as_int();
        case Fa_TypeTag::DOUBLE: return as_double() == other.as_double();
        default: return as_object() == other.as_object();
        }
    }

    bool operator!=(Fa_Value const& other) const { return !(*this == other); }
};

/* ------- Factory macros ------- */

static inline Fa_Value Fa_make_nil()
{
    return Fa_Value::nil();
}
static inline Fa_Value Fa_make_obj(Fa_ObjHeader const* p)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(p));
}
static inline Fa_Value Fa_make_real(f64 d)
{
    return Fa_Value::from_double(d);
}
static inline Fa_Value Fa_make_bool(bool b)
{
    return Fa_Value::from_bool(b);
}
static inline Fa_Value Fa_make_int(i64 v)
{
    return Fa_Value::from_int(v);
}

static inline Fa_Value Fa_from_string(Fa_ObjString const* s)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(s));
}
static inline Fa_Value Fa_from_list(Fa_ObjList const* l)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(l));
}
static inline Fa_Value Fa_from_dict(Fa_ObjDict const* d)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(d));
}
static inline Fa_Value Fa_from_func(Fa_ObjFunction const* f)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(f));
}
static inline Fa_Value Fa_from_native(Fa_ObjNative const* n)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(n));
}
static inline Fa_Value Fa_from_class(Fa_ObjClass const* c)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(c));
}
static inline Fa_Value Fa_from_instance(Fa_ObjInstance const* i)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(i));
}
static inline Fa_Value Fa_from_file_handle(Fa_ObjFileHandle const* f)
{
    return Fa_Value::from_object(reinterpret_cast<Fa_ObjHeader*>(f));
}

/* ------- Truth macros  ------- */

static inline bool Fa_is_nil(Fa_Value const v)
{
    return v.is_nil();
}
static inline bool Fa_is_bool(Fa_Value const v)
{
    return v.is_bool();
}
static inline bool Fa_is_int(Fa_Value const v)
{
    return v.is_int();
}
static inline bool Fa_is_obj(Fa_Value const v)
{
    return v.is_object();
}
static inline bool Fa_is_double(Fa_Value const v)
{
    return v.is_double();
}
static inline bool Fa_is_number(Fa_Value const v)
{
    return v.is_double() || v.is_int();
}

static inline bool Fa_is_string(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::STRING;
}
static inline bool Fa_is_list(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::LIST;
}
static inline bool Fa_is_dict(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::DICT;
}
static inline bool Fa_is_function(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::FUNCTION;
}
static inline bool Fa_is_native(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::NATIVE;
}
static inline bool Fa_is_class(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::CLASS;
}
static inline bool Fa_is_instance(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::INSTANCE;
}
static inline bool Fa_is_file_handle(Fa_Value const v)
{
    return v.is_object() && v.as_object()->type == Fa_ObjType::FILE_HANDLE;
}

static inline bool Fa_is_truthy(Fa_Value const v)
{
    if (v.is_nil())
        return false;
    else if (v.is_bool())
        return v.as_bool();
    else if (v.is_int())
        return v.as_int() != 0;
    else if (v.is_object())
        return true;
    return v.as_double() != 0.0f;
}

/* -------- Casting macros -------- */

static inline bool Fa_as_bool(Fa_Value const v) { return v.as_bool(); }
static inline i64 Fa_as_int(Fa_Value const v) { return v.as_int(); }
static inline f64 Fa_as_double(Fa_Value const v) { return v.as_double(); }
static inline f64 Fa_as_double_any(Fa_Value const v)
{
    return v.is_int() ? static_cast<f64>(v.as_int()) : v.as_double();
}

[[nodiscard]] inline Fa_TypeTag value_type_tag(Fa_Value v) noexcept
{
    return v.type_tag();
}

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const;
};

struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) const { return lhs == rhs; }
};

#endif // FA_USE_NAN_BOXING

} // namespace fairuz::runtime

#endif // FA_VALUE_HPP
