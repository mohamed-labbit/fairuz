#ifndef VALUE_HPP
#define VALUE_HPP

#include "opcode.hpp"
#include "ssa_loop.hpp"
#include "table.hpp"
#include <cstdint>

namespace fairuz::runtime {

class Fa_VM; // forward
using Fa_Value = u64;

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const noexcept { return v; }
}; // Fa_Value's are inherently distinct
struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) const noexcept { return lhs == rhs; }
};

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
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
    /* special mehtods indices */
    enum : u32 {
        INIT,
        CALL,
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        REPR,
        EQ,
        NEQ,
        NEG,
        _COUNT,
    };

    Fa_StringRef name;

    // fields
    Fa_Array<Fa_StringRef> field_names;
    u32 field_count { 0 };

    // methods
    Fa_Array<Fa_StringRef> method_names;
    Fa_Array<Fa_Chunk*> vtable;

    // loopk-up map
    using IndexTable = Fa_HashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual>;

    IndexTable field_index_map;
    IndexTable method_slot_map;

    Fa_ObjClass() = delete;

    explicit Fa_ObjClass(Fa_StringRef n, Fa_Array<Fa_StringRef> f, Fa_Array<Fa_StringRef> m, Fa_Array<Fa_Chunk*> v)
        : Fa_ObjHeader(Fa_ObjType::CLASS)
        , name(n)
        , field_names(f)
        , method_names(m)
        , vtable(v)
    {
        field_count = field_names.size();
        build_indices();
    }

    void build_indices()
    {
        for (u32 i = 0, n = field_names.size(); i < n; i++)
            field_index_map[field_names[i]] = i;

        for (u32 i = 0, n = method_names.size(); i < n; i++)
            method_slot_map[method_names[i]] = i;
    }

    // access
    int field_index(Fa_StringRef name) const
    {
        u32 const* p = field_index_map.find_ptr(name);
        return p != nullptr ? static_cast<int>(*p) : -1;
    }

    int method_slot(Fa_StringRef name) const
    {
        u32 const* p = method_slot_map.find_ptr(name);
        return p != nullptr ? static_cast<int>(*p) : -1;
    }
}; // struct Fa_ObjClass

struct Fa_ObjInstance final : public Fa_ObjHeader {
    Fa_ObjClass* klass { nullptr };
    Fa_Value* fields { nullptr };
    u32 field_count { 0 };

    Fa_ObjInstance(Fa_ObjClass* k)
        : Fa_ObjHeader(Fa_ObjType::INSTANCE)
        , klass(k)
    {
        if (klass != nullptr) {
            field_count = klass->field_names.size();
            fields = new Fa_Value[field_count];

            for (int i = 0, n = field_count; i < n; ++i)
                fields[i] = NIL_VAL;
        }
    }

    ~Fa_ObjInstance() { delete[] fields; }
};

/* ------ Factory macros  ------- */

// tagged objects
#define Fa_MAKE_OBJ_STRING(s) get_allocator().allocate_object<Fa_ObjString>(s)
#define Fa_MAKE_OBJ_NATIVE(f, n, a) get_allocator().allocate_object<Fa_ObjNative>(f, n, a)
#define Fa_MAKE_OBJ_LIST() m_gc.make<Fa_ObjList>()
#define Fa_MAKE_OBJ_DICT() m_gc.make<Fa_ObjDict>()
#define Fa_MAKE_OBJ_FUNCTION(ch) m_gc.make<Fa_ObjFunction>(ch)
#define Fa_MAKE_OBJ_CLOSURE(fn) m_gc.make<Fa_ObjClosure>(fn)
#define Fa_MAKE_OBJ_CLASS(n, f, m, v) get_allocator().allocate_object<Fa_ObjClass>(n, f, m, v)
#define Fa_MAKE_OBJ_INSTANCE(k) m_gc.make<Fa_ObjInstance>(k)

// runtime values
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
#define Fa_MAKE_CLASS(n, f, m, v)                                                     \
    ({                                                                                \
        Fa_ObjClass* _obj = get_allocator().allocate_object<Fa_ObjClass>(n, f, m, v); \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK);                 \
    })
#define Fa_MAKE_INSTANCE(k)                                           \
    ({                                                                \
        Fa_ObjInstance* _obj = m_gc.make<Fa_ObjInstance>(k);          \
        TAG_OBJ | (reinterpret_cast<uintptr_t>(_obj) & PAYLOAD_MASK); \
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
#define Fa_AS_STRING(v) static_cast<Fa_ObjString*>(Fa_AS_OBJECT(v))
#define Fa_AS_LIST(v) static_cast<Fa_ObjList*>(Fa_AS_OBJECT(v))
#define Fa_AS_DICT(v) static_cast<Fa_ObjDict*>(Fa_AS_OBJECT(v))
#define Fa_AS_FUNCTION(v) static_cast<Fa_ObjFunction*>(Fa_AS_OBJECT(v))
#define Fa_AS_CLOSURE(v) static_cast<Fa_ObjClosure*>(Fa_AS_OBJECT(v))
#define Fa_AS_NATIVE(v) static_cast<Fa_ObjNative*>(Fa_AS_OBJECT(v))
#define Fa_AS_CLASS(v) static_cast<Fa_ObjClass*>(Fa_AS_OBJECT(v))
#define Fa_AS_INSTANCE(v) static_cast<Fa_ObjInstance*>(Fa_AS_OBJECT(v))

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
        case Fa_ObjType::STRING:
            return Fa_TypeTag::STRING;
        case Fa_ObjType::LIST:
            return Fa_TypeTag::LIST;
        case Fa_ObjType::DICT:
            return Fa_TypeTag::DICT;
        case Fa_ObjType::CLOSURE:
            return Fa_TypeTag::CLOSURE;
        case Fa_ObjType::FUNCTION:
            return Fa_TypeTag::FUNCTION;
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
