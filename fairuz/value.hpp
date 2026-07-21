#ifndef VALUE_HPP
#define VALUE_HPP

#include "opcode.hpp"
#include "ssa_loop.hpp"
#include "table.hpp"
#include <cstdint>
#include <memory>

namespace fairuz::runtime {

class Fa_VM; // forward
using Fa_Value = u64;

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, util::Fa_ValueHash, util::Fa_ValueEqual>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);

/// NOTE: exclude any added tag from Fa_IS_DOUBLE macro
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
#define Fa_MAKE_OBJECT(p) TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK)
#define Fa_MAKE_REAL(d)                               \
    ({                                                \
        auto _tmp = (d);                              \
        Fa_Value bits;                                \
        __builtin_memcpy(&bits, &_tmp, sizeof(bits)); \
        bits;                                         \
    })
#define Fa_MAKE_BOOL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define Fa_MAKE_INTEGER(v) (static_cast<Fa_Value>(v) & PAYLOAD_MASK) | TAG_INT

#define Fa_MAKE_STRING(s)                                            \
    ({                                                              \
        Fa_ObjString* _o = m_gc.make_obj_string(s);                  \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_LIST()                                              \
    ({                                                              \
        Fa_ObjList* _o = m_gc.make_obj_list();                      \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_DICT()                                              \
    ({                                                              \
        Fa_ObjDict* _o = m_gc.make_obj_dict();                      \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_FUNCTION()                                          \
    ({                                                              \
        Fa_ObjFunction* _o = m_gc.make_obj_function();              \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_NATIVE(f, n, a)                                            \
    ({                                                              \
        Fa_ObjNative* _o = m_gc.make_obj_native(f, n, a);                  \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })

#define Fa_MAKE_CLASS(n, f, f_c, m, m_c, v, v_c)                                             \
    ({                                                              \
        Fa_ObjClass* _o = m_gc.make_obj_class(n, f, f_c, m, m_c, v, v_c);                    \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_INSTANCE()                                          \
    ({                                                              \
        Fa_ObjInstance* _o = m_gc.make_obj_instance();              \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_o) & PAYLOAD_MASK); \
    })

/* ------- Truth macros  ------- */

#define Fa_IS_NIL(v) ((v) == NIL_VAL)
#define Fa_IS_BOOL(v) (((v) | 1) == TRUE_VAL)
#define Fa_IS_INTEGER(v) (((v) >> 48) == INT_TAG16)
#define Fa_IS_OBJECT(v) (((v) >> 48) == OBJ_TAG16)
#define Fa_IS_DOUBLE(v)                                                                            \
    ({                                                                                             \
        u64 _top = (v) >> 48;                                                                      \
        (_top != INT_TAG16 && _top != OBJ_TAG16 && !(((v) | 1) == TRUE_VAL) && !((v) == NIL_VAL)); \
    })
#define Fa_IS_NUMBER(v)                                                                              \
    ({                                                                                               \
        u64 _top = (v) >> 48;                                                                        \
        (_top == INT_TAG16) || (_top != OBJ_TAG16 && !(((v) | 1) == TRUE_VAL) && !((v) == NIL_VAL)); \
    })
#define Fa_IS_STRING(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::STRING)
#define Fa_IS_LIST(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::LIST)
#define Fa_IS_DICT(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::DICT)
#define Fa_IS_FUNCTION(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::FUNCTION)
#define Fa_IS_CLOSURE(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::CLOSURE)
#define Fa_IS_NATIVE(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::NATIVE)
#define Fa_IS_CLASS(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::CLASS)
#define Fa_IS_INSTANCE(v) (Fa_IS_OBJECT(v) && reinterpret_cast<Fa_ObjHeader*>((v) & PAYLOAD_MASK)->type == Fa_ObjType::INSTANCE)
#define Fa_IS_TRUTHY(v)                        \
    ({                                         \
        Fa_Value _v = (v);                     \
        u64 _tag = _v >> 48;                   \
        bool _res;                             \
                                               \
        if Fa_IS_NIL (_v)                      \
            _res = false;                      \
        else if ((_v | 1) == TRUE_VAL)         \
            _res = (_v & 1);                   \
        else if (_tag == INT_TAG16)            \
            _res = ((_v & PAYLOAD_MASK) != 0); \
        else if (_tag == OBJ_TAG16)            \
            _res = true;                       \
        else                                   \
            _res = ((_v << 1) != 0);           \
                                               \
        _res;                                  \
    })

/* -------- Casting macros -------- */

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
#define Fa_AS_STRING(v) Fa_obj_cast<Fa_ObjString>(Fa_AS_OBJECT(v), Fa_ObjType::STRING)
#define Fa_AS_LIST(v) Fa_obj_cast<Fa_ObjList>(Fa_AS_OBJECT(v), Fa_ObjType::LIST)
#define Fa_AS_DICT(v) Fa_obj_cast<Fa_ObjDict>(Fa_AS_OBJECT(v), Fa_ObjType::DICT)
#define Fa_AS_FUNCTION(v) Fa_obj_cast<Fa_ObjFunction>(Fa_AS_OBJECT(v), Fa_ObjType::FUNCTION)
#define Fa_AS_NATIVE(v) Fa_obj_cast<Fa_ObjNative>(Fa_AS_OBJECT(v), Fa_ObjType::NATIVE)
#define Fa_AS_CLASS(v) Fa_obj_cast<Fa_ObjClass>(Fa_AS_OBJECT(v), Fa_ObjType::CLASS)
#define Fa_AS_INSTANCE(v) Fa_obj_cast<Fa_ObjInstance>(Fa_AS_OBJECT(v), Fa_ObjType::INSTANCE)

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
    if (Fa_IS_NIL(v))
        return Fa_TypeTag::NIL;
    if (Fa_IS_BOOL(v))
        return Fa_TypeTag::BOOL;
    if (Fa_IS_INTEGER(v))
        return Fa_TypeTag::INT;
    if (Fa_IS_DOUBLE(v))
        return Fa_TypeTag::DOUBLE;

    if (Fa_IS_OBJECT(v)) {
        switch (Fa_AS_OBJECT(v)->type) {
        case Fa_ObjType::STRING: return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST: return Fa_TypeTag::LIST;
        case Fa_ObjType::DICT: return Fa_TypeTag::DICT;
        case Fa_ObjType::FUNCTION: return Fa_TypeTag::FUNCTION;
        case Fa_ObjType::NATIVE: return Fa_TypeTag::NATIVE;
        case Fa_ObjType::CLASS: return Fa_TypeTag::CLASS;
        case Fa_ObjType::INSTANCE: return Fa_TypeTag::INSTANCE;
        default: return Fa_TypeTag::NONE;
        }
    }

    return Fa_TypeTag::NONE;
}

} // namespace fairuz::runtime

#endif // VALUE_HPP
