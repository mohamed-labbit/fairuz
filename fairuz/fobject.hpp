#pragma once

#include "array.hpp"
#include "macros.hpp"
#include "opcode.hpp"
#include "string.hpp"
#include "table.hpp"

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace fairuz::runtime {

using Fa_Value = u64;

struct Fa_ValueHash {
    size_t operator()(Fa_Value const& v) const noexcept { return v; }
};

struct Fa_ValueEqual {
    bool operator()(Fa_Value const& lhs, Fa_Value const& rhs) const noexcept { return lhs == rhs; }
};

class Fa_VM;
class Fa_Chunk;

struct _Allocator {
    template<typename T, typename... Args>
    [[nodiscard]] T* allocate_object(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
        T* mem = new T(std::forward<Args>(args)...);
        if (mem == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        return mem;
    }

    template<typename T>
    T* allocate_array(u32 const count = 1)
    {
        T* mem = new T[count] { };
        if (mem == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        return mem;
    }

    template<typename T>
    void deallocate_array(T* ptr, u32 const count)
    {
        if (ptr == nullptr)
            return;
        for (u32 i = 0; i < count; ++i)
            ptr[i].~T();
        delete[] ptr;
    }

    template<typename T>
    void deallocate_object(T* obj)
    {
        if (obj == nullptr)
            return;
        obj->~T(); delete obj;
    }
}; // _Allocator

inline _Allocator& runtime_string_allocator()
{
    static _Allocator allocator;
    return allocator;
}

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);
/// a runtime string entirely collectable by the gc
using Fa_RTStringRef = Fa_StringRefImpl<_Allocator>;
using Fa_RTStringRefHash = Fa_StringRefHashImpl<_Allocator>;
using Fa_RTStringRefEqual = Fa_StringRefEqualImpl<_Allocator>;

enum class Fa_ObjType : u8 {
    STRING,
    LIST,
    DICT,
    FUNCTION,
    NATIVE,
    CLASS,
    INSTANCE,
    _COUNT,
};

/// INVARIANT: Fa_ObjHeader is always the first member of every heap object below.
/// Code casts Fa_ObjHeader* to concrete object pointers based on this layout and the
/// runtime type tag. Do not add C++ virtual functions or inheritance to these types.
struct Fa_ObjHeader {
    Fa_ObjType type { Fa_ObjType::STRING };
    bool is_marked { false };
    Fa_ObjHeader* next { nullptr };

    Fa_ObjHeader() = default;
    Fa_ObjHeader(Fa_ObjType t)
        : type(t)
    {
    }
};

struct Fa_ObjString {
    Fa_ObjHeader obj;
    Fa_RTStringRef str;
    u64 hash { 0 };

    Fa_ObjString() = delete;

    explicit Fa_ObjString(Fa_RTStringRef s)
        : str(s)
        , hash(static_cast<u64>(std::hash<Fa_RTStringRef> { }(s)))
    {
        obj = { Fa_ObjType::STRING };
    }

    explicit Fa_ObjString(char const* s)
        : Fa_ObjString(Fa_RTStringRef(s, &runtime_string_allocator()))
    {
    }

    explicit Fa_ObjString(Fa_StringRef s)
        : Fa_ObjString(Fa_RTStringRef(s.data(), &runtime_string_allocator()))
    {
    }
};

struct Fa_ObjList {
    Fa_ObjHeader obj;
    Fa_Array<Fa_Value> elements;

    Fa_ObjList() { obj = { Fa_ObjType::LIST }; }

    void reserve(u32 cap);
    u32 size() const;
    void push(Fa_Value& v);
    bool empty() const;
};

struct Fa_ObjDict {
    Fa_ObjHeader obj;
    Fa_DictType data;

    Fa_ObjDict() { obj = { Fa_ObjType::DICT }; }

    explicit Fa_ObjDict(Fa_DictType d)
        : data(std::move(d))
    {
        obj = { Fa_ObjType::DICT };
    }
};

struct Fa_ObjFunction {
    Fa_ObjHeader obj;
    Fa_Chunk* chunk { nullptr };

    explicit Fa_ObjFunction(Fa_Chunk* ch = nullptr)
        : chunk(ch)
    {
        obj = { Fa_ObjType::FUNCTION };
    }

    Fa_StringRef name() const;
    u32 arity() const;
};

struct Fa_ObjNative {
    Fa_ObjHeader obj;
    NativeFn fn { nullptr };
    Fa_ObjString* name { nullptr };
    int arity { 0 };

    Fa_ObjNative(NativeFn f, Fa_ObjString* n, int a)
        : fn(f)
        , name(n)
        , arity(a)
    {
        assert(f != nullptr && "native function pointer must not be null");
        obj = { Fa_ObjType::NATIVE };
    }
};

struct Fa_ObjClass {
    Fa_ObjHeader obj;

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

    Fa_RTStringRef name = "";

    // Field slots are fixed once the class object is constructed. Instances keep a
    // flat array with this exact slot count; no re-classing or dynamic shape change
    // is supported by this object model.
    // Fa_Array<Fa_RTStringRef> field_names;
    Fa_RTStringRef* field_names { nullptr };
    u32 field_count { 0 };

    // Single-inheritance/method resolution is flattened by the compiler into this
    // vtable. Runtime dispatch does a direct slot lookup through method_slot_map.
    // Fa_Array<Fa_RTStringRef> method_names;
    // Fa_Array<Fa_Chunk*> vtable;

    /// NOTE: We use raw pointers so objects can be easily collected by the gc,
    /// however, methods and variables exist statically regardless of their own
    /// state, so using a fixed size dynamically allocated array won't create
    /// reallocation and out of bounds access issues anyway
    Fa_RTStringRef* method_names { nullptr };
    Fa_Chunk** vtable { nullptr };
    u32 method_count { 0 };
    u32 vtable_size { 0 };

    using IndexTable = Fa_HashTable<Fa_RTStringRef, u32, Fa_RTStringRefHash, Fa_RTStringRefEqual>;

    IndexTable field_index_map = {};
    IndexTable method_slot_map = {};

    Fa_ObjClass() = default;

    explicit Fa_ObjClass(Fa_RTStringRef n, Fa_RTStringRef* f, u32 f_c, Fa_RTStringRef* m, u32 m_c, Fa_Chunk** v, u32 v_c)
        : name(n)
        , field_names(f)
        , field_count(f_c)
        , method_names(m)
        , vtable(v)
        , method_count(m_c)
        , vtable_size(v_c)
    {
        assert(field_names != nullptr && method_names != nullptr && vtable != nullptr);
        obj = { Fa_ObjType::CLASS };
        build_indices();
    }

    void build_indices();

    int field_index(Fa_RTStringRef field_name) const;
    int method_slot(Fa_RTStringRef method_name) const;

    ~Fa_ObjClass() {
        if (field_names != nullptr)
            delete [] field_names;
        if (method_names != nullptr)
            delete [] method_names;
    }
};

struct Fa_ObjInstance {
    Fa_ObjHeader obj;
    Fa_ObjClass* klass { nullptr };
    Fa_Value* fields { nullptr };
    u32 field_count { 0 };

    explicit Fa_ObjInstance(Fa_ObjClass* k, Fa_Value* fs = nullptr, u32 fc = 0)
        : klass(k)
        , fields(fs)
        , field_count(fc)
    {
        assert(k != nullptr && "instance must be constructed with a valid class");
        obj = { Fa_ObjType::INSTANCE };
    }

    ~Fa_ObjInstance() { delete[] fields; }
};

static_assert(std::is_standard_layout_v<Fa_ObjHeader>, "Fa_ObjHeader must remain standard-layout");
static_assert(!std::is_polymorphic_v<Fa_ObjHeader>, "Fa_ObjHeader must not gain a vtable");
static_assert(offsetof(Fa_ObjString, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjString");
static_assert(offsetof(Fa_ObjList, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjList");
static_assert(offsetof(Fa_ObjDict, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjDict");
static_assert(offsetof(Fa_ObjFunction, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjFunction");
static_assert(offsetof(Fa_ObjNative, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjNative");
static_assert(offsetof(Fa_ObjClass, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjClass");
static_assert(offsetof(Fa_ObjInstance, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjInstance");

template<typename T>
inline T* Fa_obj_cast(Fa_ObjHeader* obj, Fa_ObjType expected)
{
    assert(obj != nullptr && "cannot cast a null object");
    assert(obj->type == expected && "Fa_Obj type tag mismatch on cast");
    return reinterpret_cast<T*>(obj);
}

template<typename T>
inline T const* Fa_obj_cast(Fa_ObjHeader const* obj, Fa_ObjType expected)
{
    assert(obj != nullptr && "cannot cast a null object");
    assert(obj->type == expected && "Fa_Obj type tag mismatch on cast");
    return reinterpret_cast<T const*>(obj);
}

} // namespace fairuz::runtime
