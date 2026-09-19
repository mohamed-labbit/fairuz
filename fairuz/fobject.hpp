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

struct Chunk;
class VM;
class GarbageCollector;

struct GlobalEnvironment {
    using IndexTable = HashTable<StringRef, u32, StringRefHash, StringRefEqual>;

    IndexTable index;
    Array<Value> slots;
    GlobalEnvironment* fallback { nullptr };

    Value const* find(StringRef const& name) const
    {
        if (u32 const* slot = index.find_ptr(name))
            return *slot < slots.size() ? &slots[*slot] : nullptr;
        return fallback == nullptr ? nullptr : fallback->find(name);
    }
};

using DictType = HashTable<Value, Value, ValueHash, ValueEqual>;
using NativeFn = Value (VM::*)(int, Value*);
using ListType = Array<Value, /*_Alloc=*/GarbageCollector>;

/// INVARIANT: ObjHeader is always the first member of every heap object below.
/// Code casts ObjHeader* to concrete object pointers based on this layout and the
/// runtime type tag. Do not add C++ virtual functions or inheritance to these types.

#if FA_USE_NANBOX

struct ObjInt {
    ObjHeader obj { ObjType::INT };
    i64 val { UINT64_C(0) };
};

#endif // FA_USE_NANBOX

struct ObjString {
    ObjHeader obj { ObjType::STRING };
    StringRef str = "";
    u64 hash { 0 };

    ~ObjString() { }
};

struct ObjList {
    ObjHeader obj;
    ListType elements;

    ObjList(ListType elems);

    void reserve(u32 cap);
    u32 size() const;
    void push(Value& v);
    bool empty() const;
};

struct ObjDict {
    ObjHeader obj { ObjType::DICT };
    DictType data = { };
    Array<Value> insertion_order = { };

    void set(Value key, Value value)
    {
        if (!data.contains(key))
            insertion_order.push(key);
        data.insert_or_assign(key, value);
    }

    bool erase(Value key, Value* removed = nullptr)
    {
        Value* existing = data.find_ptr(key);
        if (existing == nullptr)
            return false;
        if (removed != nullptr)
            *removed = *existing;
        if (!data.erase(key))
            return false;
        ValueEqual equal;
        for (u32 i = 0; i < insertion_order.size(); ++i) {
            if (equal(insertion_order[i], key)) {
                insertion_order.erase(i);
                break;
            }
        }
        return true;
    }
};

struct ObjFunction {
    ObjHeader obj { ObjType::FUNCTION };
    Chunk* chunk { nullptr };
    GlobalEnvironment* globals { nullptr };

    StringRef name() const;
    u32 arity() const;
};

struct ObjNative {
    ObjHeader obj { ObjType::NATIVE };
    NativeFn fn { nullptr };
    ObjString* name { nullptr };
    int arity { 0 };
};

struct ObjClass {
    using IndexTable = HashTable<StringRef, u32, StringRefHash, StringRefEqual>;

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

    ObjHeader obj { ObjType::CLASS };
    StringRef name = "";
    ObjClass* parent { nullptr };
    GlobalEnvironment* globals { nullptr };
    Array<StringRef, /*_Alloc=*/GarbageCollector> field_names;
    Array<StringRef, /*_Alloc=*/GarbageCollector> method_names;
    Array<Chunk*, /*_Alloc=*/GarbageCollector> vtable;
    IndexTable field_index_map = { };
    IndexTable method_slot_map = { };

    ObjClass(
        Array<StringRef, /*_Alloc=*/GarbageCollector> f,
        Array<StringRef, /*_Alloc=*/GarbageCollector> m,
        Array<Chunk*, /*_Alloc=*/GarbageCollector> vt);

    void build_indices();

    int field_index(StringRef field_name) const;
    int method_slot(StringRef method_name) const;
};

struct ObjModule {
    ObjHeader obj { ObjType::MODULE };
    std::string name;
    std::string path;
    GlobalEnvironment* globals { nullptr };
    Chunk* chunk { nullptr };
    bool executing { false };
    bool initialized { false };
};

struct ObjInstance {
    ObjHeader obj;
    ObjClass* klass { nullptr };
    Array<Value, /*_Alloc=*/GarbageCollector> fields;

    ObjInstance(Array<Value, /*_Alloc=*/GarbageCollector> fields);

    ~ObjInstance() = default;
};

struct ObjFileHandle {
    ObjHeader obj { ObjType::FILE_HANDLE };
    FILE* fp { nullptr };
    bool is_open { false };

    ObjFileHandle() = default;
    ObjFileHandle(ObjFileHandle const&) = delete;
    ObjFileHandle& operator=(ObjFileHandle const&) = delete;

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

    ~ObjFileHandle()
    {
        close();
    }
};

static_assert(std::is_standard_layout_v<ObjHeader>, "ObjHeader must remain standard-layout");
static_assert(!std::is_polymorphic_v<ObjHeader>, "ObjHeader must not gain a vtable");
static_assert(offsetof(ObjString, obj) == 0, "ObjHeader must be the first member of ObjString");
static_assert(offsetof(ObjList, obj) == 0, "ObjHeader must be the first member of ObjList");
static_assert(offsetof(ObjDict, obj) == 0, "ObjHeader must be the first member of ObjDict");
static_assert(offsetof(ObjFunction, obj) == 0, "ObjHeader must be the first member of ObjFunction");
static_assert(offsetof(ObjModule, obj) == 0, "ObjHeader must be the first member of ObjModule");
static_assert(offsetof(ObjNative, obj) == 0, "ObjHeader must be the first member of ObjNative");
static_assert(offsetof(ObjClass, obj) == 0, "ObjHeader must be the first member of ObjClass");
static_assert(offsetof(ObjInstance, obj) == 0, "ObjHeader must be the first member of ObjInstance");
static_assert(offsetof(ObjFileHandle, obj) == 0, "ObjHeader must be the first member of ObjFileHandle");

} // namespace fairuz::runtime

#endif // FA_OBJECT_HPP
