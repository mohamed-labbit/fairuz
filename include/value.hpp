#ifndef VALUE_HPP
#define VALUE_HPP

#include "opcode.hpp"
#include "pair.hpp"
#include <cstdint>

namespace fairuz::runtime {

class Fa_VM; // forward
using Fa_Value = u64;
using Fa_DictType = Fa_Array<Fa_Pair<Fa_Value, Fa_Value>>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);

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

enum class Fa_ObjType : u8 {
    STRING,
    LIST,
    DICT,
    FUNCTION,
    CLOSURE,
    NATIVE,
    UPVALUE
}; // enum Fa_ObjType

struct Fa_ObjHeader {
    Fa_ObjType m_type { Fa_ObjType::UPVALUE };
    bool is_marked { false };
    Fa_ObjHeader* m_next { nullptr };

    explicit Fa_ObjHeader(Fa_ObjType t) noexcept
        : m_type(t)
    {
    }
}; // struct Fa_ObjHeader

struct Fa_ObjString final : public Fa_ObjHeader {
    Fa_StringRef str;
    u64 m_hash;

    explicit Fa_ObjString(Fa_StringRef s)
        : Fa_ObjHeader(Fa_ObjType::STRING)
        , str(s)
        , m_hash(static_cast<u64>(std::hash<Fa_StringRef> { }(s)))
    {
    }
}; // struct Fa_ObjString

struct Fa_ObjList final : public Fa_ObjHeader {
    Fa_Array<Fa_Value> m_elements;

    Fa_ObjList()
        : Fa_ObjHeader(Fa_ObjType::LIST)
    {
    }

    void reserve(u32 m_cap) { m_elements.reserve(m_cap); }
    u32 size() const { return m_elements.size(); }
}; // struct Fa_ObjList

struct Fa_ObjDict final : public Fa_ObjHeader {
    Fa_DictType data;

    Fa_ObjDict() 
        : Fa_ObjHeader(Fa_ObjType::DICT)
    {
    }
};

struct Fa_ObjFunction final : public Fa_ObjHeader {
    unsigned int arity { 0 };
    unsigned int upvalue_count { 0 };
    Fa_Chunk* chunk { nullptr };
    Fa_ObjString* m_name { nullptr };

    explicit Fa_ObjFunction(Fa_Chunk* ch = nullptr)
        : Fa_ObjHeader(Fa_ObjType::FUNCTION)
        , chunk(ch)
    {
    }
}; // struct Fa_ObjFunction

struct ObjUpvalue final : public Fa_ObjHeader {
    Fa_Value* m_location { nullptr };
    Fa_Value closed;
    ObjUpvalue* next_open { nullptr };

    explicit ObjUpvalue(Fa_Value* slot) noexcept
        : Fa_ObjHeader(Fa_ObjType::UPVALUE)
        , m_location(slot)
    {
    }
}; // struct ObjUpvalue

struct Fa_ObjClosure final : public Fa_ObjHeader {
    Fa_ObjFunction* function { nullptr };
    Fa_Array<ObjUpvalue*> up_values;

    explicit Fa_ObjClosure(Fa_ObjFunction* fn)
        : Fa_ObjHeader(Fa_ObjType::CLOSURE)
        , function(fn)
        , up_values(fn->upvalue_count, nullptr)
    {
    }
}; // struct Fa_ObjClosure

struct Fa_ObjNative final : public Fa_ObjHeader {
    NativeFn fn;
    Fa_ObjString* m_name { nullptr };
    int arity;

    Fa_ObjNative(NativeFn f, Fa_ObjString* n, int a)
        : Fa_ObjHeader(Fa_ObjType::NATIVE)
        , fn(f)
        , m_name(n)
        , arity(a)
    {
    }
}; // struct Fa_ObjNative

#define Fa_MAKE_OBJ_STRING(s) get_allocator().allocate_object<Fa_ObjString>(s)
#define Fa_MAKE_OBJ_NATIVE(f, n, a) get_allocator().allocate_object<Fa_ObjNative>(f, n, a)
#define Fa_MAKE_OBJ_LIST() GC_.make<Fa_ObjList>()
#define Fa_MAKE_OBJ_DICT() GC_.make<Fa_ObjDict>()
#define Fa_MAKE_OBJ_FUNCTION(ch) GC_.make<Fa_ObjFunction>(ch)
#define Fa_MAKE_OBJ_UPVALUE(slot) GC_.make<ObjUpvalue>(slot)
#define Fa_MAKE_OBJ_CLOSURE(fn) GC_.make<Fa_ObjClosure>(fn)

#define Fa_MAKE_OBJECT(p) TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK)
#define Fa_MAKE_STRING(v)                                                   \
    ({                                                                      \
        Fa_ObjString* obj = get_allocator().allocate_object<Fa_ObjString>(v); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK);        \
    })
#define Fa_MAKE_LIST()                                               \
    ({                                                               \
        Fa_ObjList* obj = GC_.make<Fa_ObjList>();                    \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_DICT()\
    ({\
        Fa_ObjDict* obj = GC_.make<Fa_ObjDict>();\
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK);\
    })
#define Fa_MAKE_FUNCTION(ch)                                         \
    ({                                                               \
        Fa_ObjFunction* obj = GC_.make<Fa_ObjFunction>(ch);          \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_UPVALUE(slot)                                        \
    ({                                                               \
        ObjUpvalue* obj = GC_.make<ObjUpvalue>(slot);                \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_CLOSURE(fn)                                          \
    ({                                                               \
        Fa_ObjClosure* obj = GC_.make<Fa_ObjClosure>(fn);            \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_NATIVE(f, n, a)                                                   \
    ({                                                                            \
        Fa_ObjNative* obj = get_allocator().allocate_object<Fa_ObjNative>(f, n, a); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(obj) & PAYLOAD_MASK);              \
    })

#define Fa_MAKE_REAL(d)                               \
    ({                                                \
        auto _tmp = (d);                              \
        Fa_Value bits;                                \
        __builtin_memcpy(&bits, &_tmp, sizeof(bits)); \
        bits;                                         \
    })
#define Fa_MAKE_BOOL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define Fa_MAKE_INTEGER(v) (static_cast<Fa_Value>(v) & PAYLOAD_MASK) | TAG_INT

#define Fa_IS_NIL(v) ((v) == NIL_VAL)
#define Fa_IS_BOOL(v) (((v) | 1) == TRUE_VAL)
#define Fa_IS_INTEGER(v) (((v) >> 48) == INT_TAG16)
#define Fa_IS_OBJECT(v) (((v) >> 48) == OBJ_TAG16)
#define Fa_IS_DOUBLE(v)                                                                            \
    ({                                                                                             \
        Fa_Value _vd = (v);                                                                        \
        u64 _top = _vd >> 48;                                                                      \
        (_top != INT_TAG16 && _top != OBJ_TAG16 && !((_vd | 1) == TRUE_VAL) && !(_vd == NIL_VAL)); \
    })
#define Fa_IS_NUMBER(v)                                                                              \
    ({                                                                                               \
        Fa_Value _vn = (v);                                                                          \
        u64 _top = _vn >> 48;                                                                        \
        (_top == INT_TAG16) || (_top != OBJ_TAG16 && !((_vn | 1) == TRUE_VAL) && !(_vn == NIL_VAL)); \
    })
#define Fa_IS_STRING(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->m_type == Fa_ObjType::STRING)
#define Fa_IS_LIST(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->m_type == Fa_ObjType::LIST)
#define Fa_IS_FUNCTION(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->m_type == Fa_ObjType::FUNCTION)
#define Fa_IS_CLOSURE(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->m_type == Fa_ObjType::CLOSURE)
#define Fa_IS_NATIVE(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->m_type == Fa_ObjType::NATIVE)
#define Fa_IS_TRUTHY(v)                          \
    ({                                           \
        Fa_Value _v = (v);                       \
        u64 _tag = _v >> 48;                     \
        bool _res;                               \
                                                 \
        if (UNLIKELY(Fa_IS_NIL(_v)))             \
            _res = false;                        \
        else if (UNLIKELY((_v | 1) == TRUE_VAL)) \
            _res = (_v & 1);                     \
        else if (LIKELY(_tag == INT_TAG16))      \
            _res = ((_v & PAYLOAD_MASK) != 0);   \
        else if (UNLIKELY(_tag == OBJ_TAG16))    \
            _res = true;                         \
        else                                     \
            _res = LIKELY((_v << 1) != 0);       \
                                                 \
        _res;                                    \
    })

#define Fa_AS_BOOL(v) ((v) & 1)
#define Fa_AS_INTEGER(v)                          \
    ({                                            \
        i64 _payload = (i64)((v) & PAYLOAD_MASK); \
        if (_payload & (INT64_C(1) << 47))        \
            _payload |= ~PAYLOAD_MASK;            \
        _payload;                                 \
    })
#define Fa_AS_DOUBLE(v)                         \
    ({                                          \
        Fa_Value _v = (v);                      \
        f64 _d;                                 \
        __builtin_memcpy(&_d, &_v, sizeof(_d)); \
        _d;                                     \
    })
#define Fa_AS_DOUBLE_ANY(v)                         \
    ({                                              \
        Fa_Value _v = (v);                          \
        f64 _d;                                     \
        if (Fa_IS_INTEGER(_v))                      \
            _d = (f64)Fa_AS_INTEGER(_v);            \
        else                                        \
            __builtin_memcpy(&_d, &_v, sizeof(_d)); \
        _d;                                         \
    })
#define Fa_AS_OBJECT(v) reinterpret_cast<Fa_ObjHeader*>(static_cast<uintptr_t>((v) & PAYLOAD_MASK))
#define Fa_AS_STRING(v) static_cast<Fa_ObjString*>(Fa_AS_OBJECT(v))
#define Fa_AS_LIST(v) static_cast<Fa_ObjList*>(Fa_AS_OBJECT(v))
#define Fa_AS_DICT(v) static_cast<Fa_ObjDict*>(Fa_AS_OBJECT(v))
#define Fa_AS_FUNCTION(v) static_cast<Fa_ObjFunction*>(Fa_AS_OBJECT(v))
#define Fa_AS_CLOSURE(v) static_cast<Fa_ObjClosure*>(Fa_AS_OBJECT(v))
#define Fa_AS_NATIVE(v) static_cast<Fa_ObjNative*>(Fa_AS_OBJECT(v))

enum class Fa_TypeTag : u8 {
    NONE,
    NIL,
    BOOL,
    INT,
    DOUBLE,
    STRING,
    LIST,
    CLOSURE,
    NATIVE,
    DICT,
}; // enum Fa_TypeTag

[[nodiscard]] inline bool has_tag(Fa_TypeTag mask, Fa_TypeTag t) noexcept { return (static_cast<u8>(mask) & static_cast<u8>(t)) != 0; }

[[nodiscard]] inline Fa_TypeTag operator|(Fa_TypeTag a, Fa_TypeTag b) noexcept
{
    return static_cast<Fa_TypeTag>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline Fa_TypeTag& operator|=(Fa_TypeTag& a, Fa_TypeTag b) noexcept { return a = a | b; }

[[nodiscard]] inline Fa_TypeTag value_type_tag(Fa_Value v) noexcept
{
    if (Fa_IS_NIL(v))
        return Fa_TypeTag::NIL;
    if (Fa_IS_BOOL(v))
        return Fa_TypeTag::BOOL;
    if (Fa_IS_INTEGER(v))
        return Fa_TypeTag::INT;
    if (Fa_IS_DOUBLE(v))
        return Fa_TypeTag::DOUBLE;

    if (Fa_IS_OBJECT(v)) {
        switch (Fa_AS_OBJECT(v)->m_type) {
        case Fa_ObjType::STRING:
            return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST:
            return Fa_TypeTag::LIST;
        case  Fa_ObjType::DICT:
            return Fa_TypeTag::DICT;
        case Fa_ObjType::CLOSURE:
            return Fa_TypeTag::CLOSURE;
        case Fa_ObjType::NATIVE:
            return Fa_TypeTag::NATIVE;
        default:
            return Fa_TypeTag::NONE;
        }
    }
    return Fa_TypeTag::NONE;
}

} // namespace fairuz::runtime

#endif // VALUE_HPP
