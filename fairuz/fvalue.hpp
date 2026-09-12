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
struct Fa_ObjModule;

#if FA_USE_NANBOX

class Fa_VM; // forward

/// NOTE: exclude any added tag from Fa_is_double macro

class Fa_Value {
private:
    static constexpr u64 NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
    static constexpr u64 NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);
    static constexpr u64 TAG_INT = UINT64_C(0x7FF9000000000000);
    static constexpr u64 TAG_OBJ = UINT64_C(0xFFF8000000000000);
    static constexpr u64 PAYLOAD_MASK = UINT64_C(0x0000FFFFFFFFFFFF);
    static constexpr u64 NIL_VAL = UINT64_C(0x7FF8000000000001);
    static constexpr u64 FALSE_VAL = UINT64_C(0x7FF8000000000002);
    static constexpr u64 TRUE_VAL = UINT64_C(0x7FF8000000000003);
    static constexpr u64 INT_TAG16 = UINT64_C(0x7FF9);
    static constexpr u64 OBJ_TAG16 = UINT64_C(0xFFF8);
    static constexpr i64 INT48_MIN = -(1LL << 47);
    static constexpr i64 INT48_MAX = (1LL << 47) - 1;

    u64 m_value { NIL_VAL };

public:
    friend struct Fa_ValueHash;

    static Fa_Value nil() { return NIL_VAL; }
    static Fa_Value from_obj(Fa_ObjHeader const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }
    static Fa_Value from_bool(bool const b) { return b ? TRUE_VAL : FALSE_VAL; }
    static Fa_Value from_int(i64 const v) { return (static_cast<u64>(v) & PAYLOAD_MASK) | TAG_INT; }
    static constexpr i64 int_min() { return INT48_MIN; }
    static constexpr i64 int_max() { return INT48_MAX; }
    static Fa_Value from_real(f64 const d)
    {
        u64 bits;
        ::memcpy(&bits, &d, sizeof(bits));
        return bits;
    }

    static Fa_Value from_string(Fa_ObjString const* s) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(s) & PAYLOAD_MASK); }
    static Fa_Value from_list(Fa_ObjList const* l) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(l) & PAYLOAD_MASK); }
    static Fa_Value from_dict(Fa_ObjDict const* d) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(d) & PAYLOAD_MASK); }
    static Fa_Value from_func(Fa_ObjFunction const* f) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(f) & PAYLOAD_MASK); }
    static Fa_Value from_native(Fa_ObjNative const* n) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(n) & PAYLOAD_MASK); }
    static Fa_Value from_class(Fa_ObjClass const* c) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(c) & PAYLOAD_MASK); }
    static Fa_Value from_instance(Fa_ObjInstance const* i) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(i) & PAYLOAD_MASK); }
    static Fa_Value from_file_handle(Fa_ObjFileHandle const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }
    static Fa_Value from_module(Fa_ObjModule const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }

    Fa_Value() = default;

    Fa_Value(u64 v)
        : m_value(v)
    {
    }

    bool operator==(Fa_Value const& other) const { return other.m_value == m_value; }
    bool operator!=(Fa_Value const& other) const { return other.m_value != m_value; }

    Fa_ObjHeader* as_obj() const { return reinterpret_cast<Fa_ObjHeader*>(static_cast<uintptr_t>(m_value & PAYLOAD_MASK)); }

    bool is_nil() const { return m_value == NIL_VAL; }
    bool is_bool() const { return (m_value | 1) == TRUE_VAL; }
    bool is_int() const { return (m_value >> 48) == INT_TAG16; }
    bool is_obj() const { return (m_value >> 48) == OBJ_TAG16; }
    bool is_double() const
    {
        u64 top = m_value >> 48;
        return top != INT_TAG16 && top != OBJ_TAG16 && !((m_value | 1) == TRUE_VAL) && !(m_value == NIL_VAL);
    }

    bool is_number() const { return is_int() || is_double(); }
    bool is_string() const { return (is_obj() && as_obj()->type == Fa_ObjType::STRING); }
    bool is_list() const { return (is_obj() && as_obj()->type == Fa_ObjType::LIST); }
    bool is_dict() const { return (is_obj() && as_obj()->type == Fa_ObjType::DICT); }
    bool is_function() const { return (is_obj() && as_obj()->type == Fa_ObjType::FUNCTION); }
    bool is_native() const { return (is_obj() && as_obj()->type == Fa_ObjType::NATIVE); }
    bool is_class() const { return (is_obj() && as_obj()->type == Fa_ObjType::CLASS); }
    bool is_instance() const { return (is_obj() && as_obj()->type == Fa_ObjType::INSTANCE); }
    bool is_file_handle() const { return (is_obj() && as_obj()->type == Fa_ObjType::FILE_HANDLE); }
    bool is_module() const { return (is_obj() && as_obj()->type == Fa_ObjType::MODULE); }
    bool is_truthy() const
    {
        if (is_nil())
            return false;
        else if (is_bool())
            return m_value & 1;
        else if (is_int())
            return (m_value & PAYLOAD_MASK) != 0;
        else if (is_obj())
            return true;
        return (m_value << 1) != 0;
    }

    bool as_bool() const { return m_value & 1; }
    i64 as_int() const
    {
        i64 payload = static_cast<i64>(m_value & PAYLOAD_MASK);
        if (payload & (INT64_C(1) << 47))
            return payload | ~PAYLOAD_MASK;
        return payload;
    }
    f64 as_double() const
    {
        f64 d;
        ::memcpy(&d, &m_value, sizeof(d));
        return d;
    }
    f64 as_double_any() const
    {
        return is_int() ? static_cast<f64>(as_int()) : as_double();
    }

    Fa_ObjString* as_string() const;
    Fa_ObjList* as_list() const;
    Fa_ObjDict* as_dict() const;
    Fa_ObjFunction* as_func() const;
    Fa_ObjNative* as_native() const;
    Fa_ObjClass* as_class() const;
    Fa_ObjInstance* as_instance() const;
    Fa_ObjFileHandle* as_file_handle() const;
    Fa_ObjModule* as_module() const;

private:
    static bool fits_in_int48(i64 v) { return v >= INT48_MIN && v <= INT48_MAX; }
};

static_assert(sizeof(Fa_Value) == 8, "Size of Fa_Value must be 64 bits (8 bytes) when using NANBOX (FA_USE_NANBOX = 1)");

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const noexcept;
};

struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) const noexcept;
};

enum class Fa_TypeTag : u16 {
    NONE = 0,
    NIL = 1 << 0,
    BOOL = 1 << 1,
    INT = 1 << 2,
    DOUBLE = 1 << 3,
    STRING = 1 << 4,
    LIST = 1 << 5,
    FUNCTION = 1 << 7,
    NATIVE = 1 << 8,
    CLASS = 1 << 9,
    INSTANCE = 1 << 10,
    DICT = 1 << 11,
    FILE_HANDLE = 1 << 12,
    MODULE = 1 << 13,
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
    if (v.is_nil())
        return Fa_TypeTag::NIL;
    if (v.is_bool())
        return Fa_TypeTag::BOOL;
    if (v.is_int())
        return Fa_TypeTag::INT;
    if (v.is_double())
        return Fa_TypeTag::DOUBLE;

    if (v.is_obj()) {
        switch (v.as_obj()->type) {
        case Fa_ObjType::STRING: return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST: return Fa_TypeTag::LIST;
        case Fa_ObjType::DICT: return Fa_TypeTag::DICT;
        case Fa_ObjType::FUNCTION: return Fa_TypeTag::FUNCTION;
        case Fa_ObjType::NATIVE: return Fa_TypeTag::NATIVE;
        case Fa_ObjType::CLASS: return Fa_TypeTag::CLASS;
        case Fa_ObjType::INSTANCE: return Fa_TypeTag::INSTANCE;
        case Fa_ObjType::FILE_HANDLE: return Fa_TypeTag::FILE_HANDLE;
        case Fa_ObjType::MODULE: return Fa_TypeTag::MODULE;
        case Fa_ObjType::INT: return Fa_TypeTag::INT;
        case Fa_ObjType::_COUNT: return Fa_TypeTag::NONE;
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
    MODULE,
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
    static Fa_Value from_real(f64 fval)
    {
        Fa_Value v;
        v.m_type = Fa_TypeTag::DOUBLE;
        v.as.f = fval;
        return v;
    }
    static Fa_Value from_obj(Fa_ObjHeader* oval)
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
        case Fa_ObjType::MODULE: v.m_type = Fa_TypeTag::MODULE; break;
        default: v.m_type = Fa_TypeTag::NONE;
        }
        v.as.o = oval;
        return v;
    }

    bool is_nil() const { return m_type == Fa_TypeTag::NIL; }
    bool is_bool() const { return m_type == Fa_TypeTag::BOOL; }
    bool is_int() const { return m_type == Fa_TypeTag::INT; }
    bool is_double() const { return m_type == Fa_TypeTag::DOUBLE; }
    bool is_obj() const { return m_type >= Fa_TypeTag::STRING && m_type <= Fa_TypeTag::MODULE; }
    bool is_number() const { return is_double() || is_int(); }
    bool is_truthy() const
    {
        if (is_nil())
            return false;
        else if (is_bool())
            return as_bool();
        else if (is_int())
            return as_int() != 0;
        else if (is_obj())
            return true;
        return as_double() != 0.0f;
    }

    bool as_bool() const { return as.b; }
    i64 as_int() const { return as.i; }
    f64 as_double() const { return as.f; }
    f64 as_double_any() const { return is_int() ? static_cast<f64>(as_int()) : as_double(); }
    Fa_ObjHeader* as_obj() const { return as.o; }

    Fa_ObjString* as_string() const;
    Fa_ObjList* as_list() const;
    Fa_ObjDict* as_dict() const;
    Fa_ObjFunction* as_func() const;
    Fa_ObjNative* as_native() const;
    Fa_ObjClass* as_class() const;
    Fa_ObjInstance* as_instance() const;
    Fa_ObjFileHandle* as_file_handle() const;
    Fa_ObjModule* as_module() const;

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
        default: return as_obj() == other.as_obj();
        }
    }

    bool operator!=(Fa_Value const& other) const { return !(*this == other); }

    static Fa_Value from_string(Fa_ObjString* s) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(s)); }
    static Fa_Value from_list(Fa_ObjList* l) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(l)); }
    static Fa_Value from_dict(Fa_ObjDict* d) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(d)); }
    static Fa_Value from_func(Fa_ObjFunction* f) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(f)); }
    static Fa_Value from_native(Fa_ObjNative* n) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(n)); }
    static Fa_Value from_class(Fa_ObjClass* c) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(c)); }
    static Fa_Value from_instance(Fa_ObjInstance* i) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(i)); }
    static Fa_Value from_file_handle(Fa_ObjFileHandle* f) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(f)); }
    static Fa_Value from_module(Fa_ObjModule* m) { return from_obj(reinterpret_cast<Fa_ObjHeader*>(m)); }

    bool is_string() const { return is_obj() && m_type == Fa_TypeTag::STRING; }
    bool is_list() const { return is_obj() && m_type == Fa_TypeTag::LIST; }
    bool is_dict() const { return is_obj() && m_type == Fa_TypeTag::DICT; }
    bool is_function() const { return is_obj() && m_type == Fa_TypeTag::FUNCTION; }
    bool is_native() const { return is_obj() && m_type == Fa_TypeTag::NATIVE; }
    bool is_class() const { return is_obj() && m_type == Fa_TypeTag::CLASS; }
    bool is_instance() const { return is_obj() && m_type == Fa_TypeTag::INSTANCE; }
    bool is_file_handle() const { return is_obj() && m_type == Fa_TypeTag::FILE_HANDLE; }
    bool is_module() const { return is_obj() && m_type == Fa_TypeTag::MODULE; }
};

[[nodiscard]] inline Fa_TypeTag value_type_tag(Fa_Value v) noexcept
{
    return v.type_tag();
}

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const noexcept;
};

struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) const noexcept;
};

#endif // FA_USE_NAN_BOXING

} // namespace fairuz::runtime

#endif // FA_VALUE_HPP
