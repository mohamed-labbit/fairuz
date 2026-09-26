#ifndef FA_VALUE_HPP
#define FA_VALUE_HPP

#include "fmacros.hpp"
#include "fobj_header.hpp"

#include <cassert>
#include <cstdlib>

#if FA_USE_NANBOX

#    include <cstdint>
#    include <cstring>

#endif

namespace fairuz::runtime {

// forward
struct ObjString;
struct ObjList;
struct ObjDict;
struct ObjFunction;
struct ObjNative;
struct ObjClass;
struct ObjInstance;
struct ObjFileHandle;
struct ObjModule;
struct ObjBigInt;

class GarbageCollector;

#if FA_USE_NANBOX

class VM; // forward

/// NOTE: exclude any added tag from is_double macro

class Value {
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
    friend struct ValueHash;

    u64 value() const { return m_value; }

    static Value nil() { return NIL_VAL; }
    static Value from_obj(ObjHeader const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }
    static Value from_bool(bool const b) { return b ? TRUE_VAL : FALSE_VAL; }
    static Value from_int(i64 const v, GarbageCollector& gc);
    // Unchecked packing: callers must prove signed 48-bit representability.
    // Native inputs and expression results use the GC overload instead.
    static Value from_int(i64 const v)
    {
        assert(fits_in_int48(v));
        return (static_cast<u64>(v) & PAYLOAD_MASK) | TAG_INT;
    }
    static constexpr i64 int_min() { return INT48_MIN; }
    static constexpr i64 int_max() { return INT48_MAX; }
    static Value from_real(f64 const d)
    {
        u64 bits;
        ::memcpy(&bits, &d, sizeof(bits));
        return bits;
    }

    static Value from_string(ObjString const* s) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(s) & PAYLOAD_MASK); }
    static Value from_list(ObjList const* l) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(l) & PAYLOAD_MASK); }
    static Value from_dict(ObjDict const* d) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(d) & PAYLOAD_MASK); }
    static Value from_func(ObjFunction const* f) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(f) & PAYLOAD_MASK); }
    static Value from_native(ObjNative const* n) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(n) & PAYLOAD_MASK); }
    static Value from_class(ObjClass const* c) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(c) & PAYLOAD_MASK); }
    static Value from_instance(ObjInstance const* i) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(i) & PAYLOAD_MASK); }
    static Value from_file_handle(ObjFileHandle const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }
    static Value from_module(ObjModule const* p) { return TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK); }

    Value() = default;

    Value(u64 v)
        : m_value(v)
    {
    }

    ObjHeader* as_obj() const { return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(m_value & PAYLOAD_MASK)); }

    bool is_nil() const { return m_value == NIL_VAL; }
    bool is_bool() const { return (m_value | 1) == TRUE_VAL; }
    bool is_int() const
    {
        if (is_big_int())
            return true;
        return (m_value >> 48) == INT_TAG16;
    }
    bool is_obj() const { return (m_value >> 48) == OBJ_TAG16; }
    bool is_double() const
    {
        u64 top = m_value >> 48;
        return top != INT_TAG16 && top != OBJ_TAG16 && !((m_value | 1) == TRUE_VAL) && !(m_value == NIL_VAL);
    }

    bool is_number() const { return is_int() || is_double(); }
    bool is_string() const { return (is_obj() && as_obj()->type == ObjType::STRING); }
    bool is_list() const { return (is_obj() && as_obj()->type == ObjType::LIST); }
    bool is_dict() const { return (is_obj() && as_obj()->type == ObjType::DICT); }
    bool is_function() const { return (is_obj() && as_obj()->type == ObjType::FUNCTION); }
    bool is_native() const { return (is_obj() && as_obj()->type == ObjType::NATIVE); }
    bool is_class() const { return (is_obj() && as_obj()->type == ObjType::CLASS); }
    bool is_instance() const { return (is_obj() && as_obj()->type == ObjType::INSTANCE); }
    bool is_file_handle() const { return (is_obj() && as_obj()->type == ObjType::FILE_HANDLE); }
    bool is_module() const { return (is_obj() && as_obj()->type == ObjType::MODULE); }
    bool is_big_int() const { return (is_obj() && as_obj()->type == ObjType::INT); }
    bool is_truthy() const;

    bool as_bool() const { return m_value & 1; }
    // Checked native extraction; arbitrary-size arithmetic must use integer::* instead.
    i64 as_int() const;
    f64 as_double() const
    {
        f64 d;
        ::memcpy(&d, &m_value, sizeof(d));
        return d;
    }
    f64 as_double_any() const;

    ObjString* as_string() const;
    ObjList* as_list() const;
    ObjDict* as_dict() const;
    ObjFunction* as_func() const;
    ObjNative* as_native() const;
    ObjClass* as_class() const;
    ObjInstance* as_instance() const;
    ObjFileHandle* as_file_handle() const;
    ObjModule* as_module() const;
    ObjBigInt* as_big_int() const;

    static bool fits_in_int48(i64 v) { return v >= INT48_MIN && v <= INT48_MAX; }

private:
};

static_assert(sizeof(Value) == 8, "Size of Value must be 64 bits (8 bytes) when using NANBOX (FA_USE_NANBOX = 1)");

struct ValueHash {
    size_t operator()(Value const& v) const;
};

struct ValueEqual {
    bool operator()(Value const& lhs, Value const& rhs) const;
};

enum class TypeTag : u16 {
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
}; // enum TypeTag

[[nodiscard]] inline bool has_tag(TypeTag mask, TypeTag t) noexcept
{
    return (static_cast<u16>(mask) & static_cast<u16>(t)) != 0;
}

[[nodiscard]] inline TypeTag operator|(TypeTag a, TypeTag b) noexcept
{
    return static_cast<TypeTag>(static_cast<u16>(a) | static_cast<u16>(b));
}

inline TypeTag& operator|=(TypeTag& a, TypeTag b) noexcept { return a = a | b; }

[[nodiscard]] inline TypeTag value_type_tag(Value v) noexcept
{
    if (v.is_nil())
        return TypeTag::NIL;
    if (v.is_bool())
        return TypeTag::BOOL;
    if (v.is_int())
        return TypeTag::INT;
    if (v.is_double())
        return TypeTag::DOUBLE;

    if (v.is_obj()) {
        switch (v.as_obj()->type) {
        case ObjType::STRING: return TypeTag::STRING;
        case ObjType::LIST: return TypeTag::LIST;
        case ObjType::DICT: return TypeTag::DICT;
        case ObjType::FUNCTION: return TypeTag::FUNCTION;
        case ObjType::NATIVE: return TypeTag::NATIVE;
        case ObjType::CLASS: return TypeTag::CLASS;
        case ObjType::INSTANCE: return TypeTag::INSTANCE;
        case ObjType::FILE_HANDLE: return TypeTag::FILE_HANDLE;
        case ObjType::MODULE: return TypeTag::MODULE;
        case ObjType::INT: return TypeTag::INT;
        case ObjType::_COUNT: return TypeTag::NONE;
        }
    }

    return TypeTag::NONE;
}

#else

enum class TypeTag : u16 {
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
}; // enum TypeTag

class Value {
private:
    TypeTag m_type;

    union {
        bool b;
        i64 i;
        f64 f;
        ObjHeader* o;
    } as;

public:
    TypeTag type_tag() const { return m_type; }

    static Value nil()
    {
        Value v;
        v.m_type = TypeTag::NIL;
        return v;
    }
    static Value from_bool(bool bval)
    {
        Value v;
        v.m_type = TypeTag::BOOL;
        v.as.b = bval;
        return v;
    }
    static Value from_int(i64 ival)
    {
        Value v;
        v.m_type = TypeTag::INT;
        v.as.i = ival;
        return v;
    }
    static Value from_real(f64 fval)
    {
        Value v;
        v.m_type = TypeTag::DOUBLE;
        v.as.f = fval;
        return v;
    }
    static Value from_obj(ObjHeader* oval)
    {
        assert(oval != nullptr);
        Value v;
        switch (oval->type) {
        case ObjType::CLASS: v.m_type = TypeTag::CLASS; break;
        case ObjType::DICT: v.m_type = TypeTag::DICT; break;
        case ObjType::FUNCTION: v.m_type = TypeTag::FUNCTION; break;
        case ObjType::INSTANCE: v.m_type = TypeTag::INSTANCE; break;
        case ObjType::LIST: v.m_type = TypeTag::LIST; break;
        case ObjType::NATIVE: v.m_type = TypeTag::NATIVE; break;
        case ObjType::STRING: v.m_type = TypeTag::STRING; break;
        case ObjType::FILE_HANDLE: v.m_type = TypeTag::FILE_HANDLE; break;
        case ObjType::MODULE: v.m_type = TypeTag::MODULE; break;
        default: v.m_type = TypeTag::NONE;
        }
        v.as.o = oval;
        return v;
    }

    bool is_nil() const { return m_type == TypeTag::NIL; }
    bool is_bool() const { return m_type == TypeTag::BOOL; }
    bool is_int() const { return m_type == TypeTag::INT; }
    bool is_double() const { return m_type == TypeTag::DOUBLE; }
    bool is_obj() const { return m_type >= TypeTag::STRING && m_type <= TypeTag::MODULE; }
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
    ObjHeader* as_obj() const { return as.o; }

    ObjString* as_string() const;
    ObjList* as_list() const;
    ObjDict* as_dict() const;
    ObjFunction* as_func() const;
    ObjNative* as_native() const;
    ObjClass* as_class() const;
    ObjInstance* as_instance() const;
    ObjFileHandle* as_file_handle() const;
    ObjModule* as_module() const;

    bool operator==(Value const& other) const
    {
        if (m_type != other.m_type)
            return false;
        switch (m_type) {
        case TypeTag::NONE: return false;
        case TypeTag::NIL: return true;
        case TypeTag::BOOL: return as_bool() == other.as_bool();
        case TypeTag::INT: return as_int() == other.as_int();
        case TypeTag::DOUBLE: return as_double() == other.as_double();
        default: return as_obj() == other.as_obj();
        }
    }

    bool operator!=(Value const& other) const { return !(*this == other); }

    static Value from_string(ObjString* s) { return from_obj(reinterpret_cast<ObjHeader*>(s)); }
    static Value from_list(ObjList* l) { return from_obj(reinterpret_cast<ObjHeader*>(l)); }
    static Value from_dict(ObjDict* d) { return from_obj(reinterpret_cast<ObjHeader*>(d)); }
    static Value from_func(ObjFunction* f) { return from_obj(reinterpret_cast<ObjHeader*>(f)); }
    static Value from_native(ObjNative* n) { return from_obj(reinterpret_cast<ObjHeader*>(n)); }
    static Value from_class(ObjClass* c) { return from_obj(reinterpret_cast<ObjHeader*>(c)); }
    static Value from_instance(ObjInstance* i) { return from_obj(reinterpret_cast<ObjHeader*>(i)); }
    static Value from_file_handle(ObjFileHandle* f) { return from_obj(reinterpret_cast<ObjHeader*>(f)); }
    static Value from_module(ObjModule* m) { return from_obj(reinterpret_cast<ObjHeader*>(m)); }

    bool is_string() const { return is_obj() && m_type == TypeTag::STRING; }
    bool is_list() const { return is_obj() && m_type == TypeTag::LIST; }
    bool is_dict() const { return is_obj() && m_type == TypeTag::DICT; }
    bool is_function() const { return is_obj() && m_type == TypeTag::FUNCTION; }
    bool is_native() const { return is_obj() && m_type == TypeTag::NATIVE; }
    bool is_class() const { return is_obj() && m_type == TypeTag::CLASS; }
    bool is_instance() const { return is_obj() && m_type == TypeTag::INSTANCE; }
    bool is_file_handle() const { return is_obj() && m_type == TypeTag::FILE_HANDLE; }
    bool is_module() const { return is_obj() && m_type == TypeTag::MODULE; }
};

[[nodiscard]] inline TypeTag value_type_tag(Value v) noexcept
{
    return v.type_tag();
}

struct ValueHash {
    size_t operator()(Value const& v) const;
};

struct ValueEqual {
    bool operator()(Value const& lhs, Value const& rhs) const;
};

#endif // FA_USE_NAN_BOXING

} // namespace fairuz::runtime

#endif // FA_VALUE_HPP
