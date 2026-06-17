#ifndef VALUE_HPP
#define VALUE_HPP

#include "opcode.hpp"
#include "table.hpp"
#include <cstdint>

namespace fairuz::runtime {

class Fa_VM; // forward
using Fa_Value = u64;

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const { return v; }
}; // Fa_Value's are inherently distinct
struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) { return lhs == rhs; }
};

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
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
    CLASS,
    INSTANCE,
    _COUNT,
}; // enum Fa_ObjType

struct Fa_ObjHeader {
    Fa_ObjType type { Fa_ObjType::STRING };
    bool is_marked { false };
    Fa_ObjHeader* next { nullptr };

    explicit Fa_ObjHeader(Fa_ObjType t) noexcept
        : type(t)
    {
    }
}; // struct Fa_ObjHeader

struct Fa_ObjString final : public Fa_ObjHeader {
    Fa_StringRef str;
    u64 hash;

    explicit Fa_ObjString(Fa_StringRef s)
        : Fa_ObjHeader(Fa_ObjType::STRING)
        , str(s)
        , hash(static_cast<u64>(std::hash<Fa_StringRef> { }(s)))
    {
    }
}; // struct Fa_ObjString

struct Fa_ObjList final : public Fa_ObjHeader {
    Fa_Array<Fa_Value> elements;

    Fa_ObjList()
        : Fa_ObjHeader(Fa_ObjType::LIST)
    {
    }

    void reserve(u32 cap) { elements.reserve(cap); }
    u32 size() const { return elements.size(); }
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
    Fa_Chunk* chunk { nullptr };
    Fa_ObjString* name { nullptr };

    explicit Fa_ObjFunction(Fa_Chunk* ch = nullptr)
        : Fa_ObjHeader(Fa_ObjType::FUNCTION)
        , chunk(ch)
    {
    }
}; // struct Fa_ObjFunction

struct Fa_ObjClosure final : public Fa_ObjHeader {
    Fa_ObjFunction* function { nullptr };

    explicit Fa_ObjClosure(Fa_ObjFunction* fn)
        : Fa_ObjHeader(Fa_ObjType::CLOSURE)
        , function(fn)
    {
    }
}; // struct Fa_ObjClosure

struct Fa_ObjNative final : public Fa_ObjHeader {
    NativeFn fn;
    Fa_ObjString* name { nullptr };
    int arity;

    Fa_ObjNative(NativeFn f, Fa_ObjString* n, int a)
        : Fa_ObjHeader(Fa_ObjType::NATIVE)
        , fn(f)
        , name(n)
        , arity(a)
    {
    }
}; // struct Fa_ObjNative

struct Fa_ObjClass final : public Fa_ObjHeader {
    Fa_ObjString* name { nullptr };
    Fa_Array<Fa_ObjHeader*> members;

    // special methods
    i16 init_idx { -1 };
    i16 call_idx { -1 };
    i16 repr_idx { -1 };
    i16 add_idx { -1 };
    i16 sub_idx { -1 };
    i16 neg_idx { -1 };
    i16 mul_idx { -1 };
    i16 div_idx { -1 };
    i16 mod_idx { -1 };

    bool has_init() const { return init_idx != -1; }
    bool has_call() const { return call_idx != -1; }
    bool has_repr() const { return repr_idx != -1; }
    bool has_add() const { return add_idx != -1; }
    bool has_sub() const { return sub_idx != -1; }
    bool has_neg() const { return neg_idx != -1; }
    bool has_mul() const { return mul_idx != -1; }
    bool has_div() const { return div_idx != -1; }
    bool has_mod() const { return mod_idx != -1; }

    explicit Fa_ObjClass(Fa_ObjString* class_name, Fa_Array<Fa_ObjHeader*> member_list)
        : Fa_ObjHeader(Fa_ObjType::CLASS)
        , name(class_name)
        , members(member_list)
    {
        Fa_Array<Fa_StringRef> sp_methods_names = {
            "بداية",
            "نداء",
            "كتابة",
            "عملية+",
            "عملية-",
            "عملية*",
            "عملية/",
            /// TODO: others
        };

        u32 i = 0;
        for (; i < members.size(); ++i) {
            if (members[i]->type == Fa_ObjType::FUNCTION) {
                auto method = static_cast<Fa_ObjFunction const*>(members[i]);
                Fa_StringRef method_name = method->name->str;
                if (std::find(sp_methods_names.begin(), sp_methods_names.end(), method_name) != sp_methods_names.end()) {
                    if (method_name == "بداية")
                        init_idx = i;
                    else if (method_name == "نداء")
                        call_idx = i;
                    else if (method_name == "كتابة")
                        repr_idx = i;
                    else if (method_name == "عملية+")
                        add_idx = i;
                    else if (method_name == "عملية-")
                        sub_idx = i;
                    else if (method_name == "عملية*")
                        mul_idx = i;
                    else if (method_name == "عملية/")
                        div_idx = i;
                }
            }
        }
    }
}; // struct Fa_ObjClass

struct Fa_ObjInstance final : public Fa_ObjHeader {
    Fa_ObjClass* kclass;
    Fa_DictType fields;

    Fa_ObjInstance(Fa_ObjClass* k, Fa_DictType f)
        : Fa_ObjHeader(Fa_ObjType::INSTANCE)
        , kclass(k)
        , fields(f)
    {
    }

    bool has_init() const { return kclass->has_init(); }
    bool has_call() const { return kclass->has_call(); }
    bool has_repr() const { return kclass->has_repr(); }
    bool has_add() const { return kclass->has_add(); }
    bool has_sub() const { return kclass->has_sub(); }
    bool has_neg() const { return kclass->has_neg(); }
    bool has_mul() const { return kclass->has_mul(); }
    bool has_div() const { return kclass->has_div(); }
    bool has_mod() const { return kclass->has_mod(); }
};

#define Fa_MAKE_OBJ_STRING(s) get_allocator().allocate_object<Fa_ObjString>(s)
#define Fa_MAKE_OBJ_NATIVE(f, n, a) get_allocator().allocate_object<Fa_ObjNative>(f, n, a)
#define Fa_MAKE_OBJ_LIST() m_gc.make<Fa_ObjList>()
#define Fa_MAKE_OBJ_DICT() m_gc.make<Fa_ObjDict>()
#define Fa_MAKE_OBJ_FUNCTION(ch) m_gc.make<Fa_ObjFunction>(ch)
#define Fa_MAKE_OBJ_CLOSURE(fn) m_gc.make<Fa_ObjClosure>(fn)
#define Fa_MAKE_OBJ_CLASS(n, m) m_gc.make<Fa_ObjClass>(n, m)
#define Fa_MAKE_OBJ_INSTANCE(k, f) m_gc.make<Fa_ObjInstance>(k, f)

#define Fa_MAKE_OBJECT(p) TAG_OBJ | (reinterpret_cast<uintptr_t>(p) & PAYLOAD_MASK)
#define Fa_MAKE_STRING(v)                                                      \
    ({                                                                         \
        Fa_ObjString* _obj = get_allocator().allocate_object<Fa_ObjString>(v); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK);          \
    })
#define Fa_MAKE_LIST()                                                \
    ({                                                                \
        Fa_ObjList* _obj = m_gc.make<Fa_ObjList>();                   \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_DICT()                                                \
    ({                                                                \
        Fa_ObjDict* _obj = m_gc.make<Fa_ObjDict>();                   \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_FUNCTION(ch)                                          \
    ({                                                                \
        Fa_ObjFunction* _obj = m_gc.make<Fa_ObjFunction>(ch);         \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_CLOSURE(fn)                                           \
    ({                                                                \
        Fa_ObjClosure* _obj = m_gc.make<Fa_ObjClosure>(fn);           \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK); \
    })
#define Fa_MAKE_NATIVE(f, n, a)                                                      \
    ({                                                                               \
        Fa_ObjNative* _obj = get_allocator().allocate_object<Fa_ObjNative>(f, n, a); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK);                \
    })
#define Fa_MAKE_CLASS(n, m)                                                     \
    ({                                                                          \
        Fa_ObjClass* _obj = get_allocator().allocate_object<Fa_ObjClass>(n, m); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK);           \
    })
#define Fa_MAKE_INSANCE(k, f)                                                         \
    ({                                                                                \
        Fa_ObjInstance* _obj = get_allocator().allocate_object<Fa_ObjInstance>(k, f); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK);                 \
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
#define Fa_AS_CLASS(v) static_cast<Fa_ObjClass*>(Fa_AS_OBJECT(v))
#define Fa_AS_INSTANCE(v) static_cast<Fa_ObjInstance*>(Fa_AS_OBJECT(v))

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
    CLASS,
    INSTANCE,
    DICT,
}; // enum Fa_TypeTag

[[nodiscard]] inline bool has_tag(Fa_TypeTag mask, Fa_TypeTag t) noexcept 
{
    return (static_cast<u8>(mask) & static_cast<u8>(t)) != 0;
}

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
        switch (Fa_AS_OBJECT(v)->type) {
        case Fa_ObjType::STRING:
            return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST:
            return Fa_TypeTag::LIST;
        case Fa_ObjType::DICT:
            return Fa_TypeTag::DICT;
        case Fa_ObjType::CLOSURE:
            return Fa_TypeTag::CLOSURE;
        case Fa_ObjType::NATIVE:
            return Fa_TypeTag::NATIVE;
        case Fa_ObjType::CLASS:
            return Fa_TypeTag::CLASS;
        case Fa_ObjType::INSTANCE:
            return Fa_TypeTag::INSTANCE;
        default:
            return Fa_TypeTag::NONE;
        }
    }

    return Fa_TypeTag::NONE;
}

} // namespace fairuz::runtime

#endif // VALUE_HPP
