#pragma once

#include "farray.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"
#include "ftable.hpp"
#include "fvalue.hpp"

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace fairuz::runtime {

class Fa_VM;
class Fa_Chunk;
class Fa_GarbageCollector;

using Fa_DictType = Fa_HashTable<Fa_Value, Fa_Value, Fa_ValueHash, Fa_ValueEqual>;
using NativeFn = Fa_Value (Fa_VM::*)(int, Fa_Value*);
using Fa_ListType = Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector>;

/// INVARIANT: Fa_ObjHeader is always the first member of every heap object below.
/// Code casts Fa_ObjHeader* to concrete object pointers based on this layout and the
/// runtime type tag. Do not add C++ virtual functions or inheritance to these types.

struct Fa_ObjString {
    Fa_ObjHeader obj;
    Fa_StringRef str;
    u64 hash { 0 };

    Fa_ObjString() { obj = Fa_ObjHeader { Fa_ObjType::STRING }; }
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
    Fa_ObjHeader obj;
    Fa_DictType data;

    Fa_ObjDict() { obj = Fa_ObjHeader { Fa_ObjType::DICT }; }
};

struct Fa_ObjFunction {
    Fa_ObjHeader obj;
    Fa_Chunk* chunk { nullptr };

    Fa_ObjFunction() { obj = Fa_ObjHeader { Fa_ObjType::FUNCTION }; }

    Fa_StringRef name() const;
    u32 arity() const;
};

struct Fa_ObjNative {
    Fa_ObjHeader obj;
    NativeFn fn { nullptr };
    Fa_ObjString* name { nullptr };
    int arity { 0 };

    Fa_ObjNative() { obj = Fa_ObjHeader { Fa_ObjType::NATIVE }; }
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

    Fa_StringRef name = "";

    // Field slots are fixed once the class object is constructed. Instances keep a
    // flat array with this exact slot count; no re-classing or dynamic shape change
    // is supported by this object model.
    // Fa_Array<Fa_StringRef> field_names;
    Fa_StringRef* field_names { nullptr };
    u32 field_count { 0 };

    // Single-inheritance/method resolution is flattened by the compiler into this
    // vtable. Runtime dispatch does a direct slot lookup through method_slot_map.
    // Fa_Array<Fa_StringRef> method_names;
    // Fa_Array<Fa_Chunk*> vtable;

    /// NOTE: We use raw pointers so objects can be easily collected by the gc,
    /// however, methods and variables exist statically regardless of their own
    /// state, so using a fixed size dynamically allocated array won't create
    /// reallocation and out of bounds access issues anyway
    Fa_StringRef* method_names { nullptr };
    Fa_Chunk** vtable { nullptr };
    u32 method_count { 0 };
    u32 vtable_size { 0 };

    using IndexTable = Fa_HashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual>;

    IndexTable field_index_map = { };
    IndexTable method_slot_map = { };

    Fa_ObjClass() { obj = Fa_ObjHeader { Fa_ObjType::CLASS }; }

    void build_indices();

    int field_index(Fa_StringRef field_name) const;
    int method_slot(Fa_StringRef method_name) const;

    ~Fa_ObjClass()
    {
        if (field_names != nullptr)
            delete[] field_names;
        if (method_names != nullptr)
            delete[] method_names;
    }
};

struct Fa_ObjInstance {
    Fa_ObjHeader obj;
    Fa_ObjClass* klass { nullptr };
    Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields;

    Fa_ObjInstance(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields);

    ~Fa_ObjInstance() = default;
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
