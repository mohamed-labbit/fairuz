#ifndef FA_OBJECT_HPP
#define FA_OBJECT_HPP

#include "farray.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fstring.hpp"
#include "ftable.hpp"
#include "fvalue.hpp"

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace fairuz::runtime {

struct Fa_Chunk;
class Fa_VM;
class Fa_GarbageCollector;

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);
using Fa_ListType = Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector>;

/// INVARIANT: Fa_ObjHeader is always the first member of every heap object below.
/// Code casts Fa_ObjHeader* to concrete object pointers based on this layout and the
/// runtime type tag. Do not add C++ virtual functions or inheritance to these types.

#if FA_USE_NANBOX

struct Fa_ObjInt {
    Fa_ObjHeader obj { Fa_ObjType::INT };
    i64 val { UINT64_C(0) };
};

#endif // FA_USE_NANBOX

struct Fa_ObjString {
    Fa_ObjHeader obj { Fa_ObjType::STRING };
    Fa_StringRef str = "";
    u64 hash { 0 };
};

struct Fa_ObjList {
    Fa_ObjHeader obj;
    Fa_ListType elements;

    Fa_ObjList(Fa_ListType elems);

    void reserve(u32 cap);
    u32 size() const;
    void push(Fa_Value& v);
    bool empty() const;
};

struct Fa_ObjDict {
    Fa_ObjHeader obj { Fa_ObjType::DICT };
    Fa_DictType data = { };
};

struct Fa_ObjFunction {
    Fa_ObjHeader obj { Fa_ObjType::FUNCTION };
    Fa_Chunk* chunk { nullptr };

    Fa_StringRef name() const;
    u32 arity() const;
};

struct Fa_ObjNative {
    Fa_ObjHeader obj { Fa_ObjType::NATIVE };
    NativeFn fn { nullptr };
    Fa_ObjString* name { nullptr };
    int arity { 0 };
};

struct Fa_ObjClass {
    using IndexTable = Fa_HashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual>;

    enum : u32 {
        INIT,
        CALL,
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        NEG,
        EQ,
        NEQ,
        LT,
        LTE,
        GT,
        GTE,
        REPR,
        _COUNT,
    };

    Fa_ObjHeader obj { Fa_ObjType::CLASS };
    Fa_StringRef name = "";
    Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> field_names;
    Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> method_names;
    Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vtable;
    IndexTable field_index_map = { };
    IndexTable method_slot_map = { };

    Fa_ObjClass(
        Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> f,
        Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> m,
        Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vt);

    void build_indices();

    int field_index(Fa_StringRef field_name) const;
    int method_slot(Fa_StringRef method_name) const;
};

struct Fa_ObjInstance {
    Fa_ObjHeader obj;
    Fa_ObjClass* klass { nullptr };
    Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields;

    Fa_ObjInstance(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields);

    ~Fa_ObjInstance() = default;
};

struct Fa_ObjFileHandle {
    Fa_ObjHeader obj { Fa_ObjType::FILE_HANDLE };
    FILE* fp { nullptr };
    bool is_open { false };

    Fa_ObjFileHandle() = default;
    Fa_ObjFileHandle(Fa_ObjFileHandle const&) = delete;
    Fa_ObjFileHandle& operator=(Fa_ObjFileHandle const&) = delete;

    bool close()
    {
        if (is_open == false)
            return true;

        if (fp == nullptr)
            return false;

        bool const ok = (::fclose(fp) == 0);
        fp = nullptr;
        is_open = false;
        return ok;
    }

    ~Fa_ObjFileHandle()
    {
        close();
    }
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
static_assert(offsetof(Fa_ObjFileHandle, obj) == 0, "Fa_ObjHeader must be the first member of Fa_ObjFileHandle");

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

#if FA_USE_NANBOX

static inline Fa_ObjString* Fa_as_string(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjString>(Fa_as_obj(v), Fa_ObjType::STRING);
}
static inline Fa_ObjList* Fa_as_list(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjList>(Fa_as_obj(v), Fa_ObjType::LIST);
}
static inline Fa_ObjDict* Fa_as_dict(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjDict>(Fa_as_obj(v), Fa_ObjType::DICT);
}
static inline Fa_ObjFunction* Fa_as_func(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjFunction>(Fa_as_obj(v), Fa_ObjType::FUNCTION);
}
static inline Fa_ObjNative* Fa_as_native(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjNative>(Fa_as_obj(v), Fa_ObjType::NATIVE);
}
static inline Fa_ObjClass* Fa_as_class(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjClass>(Fa_as_obj(v), Fa_ObjType::CLASS);
}
static inline Fa_ObjInstance* Fa_as_instance(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjInstance>(Fa_as_obj(v), Fa_ObjType::INSTANCE);
}
static inline Fa_ObjFileHandle* Fa_as_file_handle(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjFileHandle>(Fa_as_obj(v), Fa_ObjType::FILE_HANDLE);
}

#else

static inline Fa_ObjHeader* Fa_as_obj(Fa_Value const v)
{
    return v.as_object();
}
static inline Fa_ObjString* Fa_as_string(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjString>((v).as_object(), Fa_ObjType::STRING);
}
static inline Fa_ObjList* Fa_as_list(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjList>((v).as_object(), Fa_ObjType::LIST);
}
static inline Fa_ObjDict* Fa_as_dict(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjDict>((v).as_object(), Fa_ObjType::DICT);
}
static inline Fa_ObjFunction* Fa_as_func(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjFunction>((v).as_object(), Fa_ObjType::FUNCTION);
}
static inline Fa_ObjNative* Fa_as_native(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjNative>((v).as_object(), Fa_ObjType::NATIVE);
}
static inline Fa_ObjClass* Fa_as_class(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjClass>((v).as_object(), Fa_ObjType::CLASS);
}
static inline Fa_ObjInstance* Fa_as_instance(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjInstance>((v).as_object(), Fa_ObjType::INSTANCE);
}
static inline Fa_ObjFileHandle* Fa_as_file_handle(Fa_Value const v)
{
    return Fa_obj_cast<Fa_ObjFileHandle>((v).as_object(), Fa_ObjType::FILE_HANDLE);
}

#endif // FA_USE_NANBOX

} // namespace fairuz::runtime

#endif // FA_OBJECT_HPP
