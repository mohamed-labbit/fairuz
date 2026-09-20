//
// fobject.cc
//

#include "fobject.hpp"
#include "fbuiltins.hpp"
#include "fgc.hpp"
#include "fopcode.hpp"

namespace fairuz::runtime {

Value const* GlobalEnvironment::find(StringRef const& name) const
{
    if (u32 const* slot = index.find_ptr(name))
        return *slot < slots.size() ? &slots[*slot] : nullptr;
    if (builtins != nullptr) {
        if (auto* value = builtins->find(name))
            return value;
    }
    return fallback == nullptr ? nullptr : fallback->find(name);
}

// object defs

ObjList::ObjList(ListType elems)
    : elements(std::move(elems))
{
    obj = ObjHeader { ObjType::LIST };
}
void ObjList::reserve(u32 cap) { elements.reserve(cap); }
u32 ObjList::size() const { return elements.size(); }
void ObjList::push(Value& v) { elements.push(v); }
bool ObjList::empty() const { return elements.empty(); }

StringRef ObjFunction::name() const { return chunk->name; }
u32 ObjFunction::arity() const { return chunk->arity; }

ObjClass::ObjClass(
    Array<StringRef, /*_Alloc=*/GarbageCollector> f,
    Array<StringRef, /*_Alloc=*/GarbageCollector> m,
    Array<Chunk*, /*_Alloc=*/GarbageCollector> vt)
    : field_names(f)
    , method_names(m)
    , vtable(vt)
{
}

void ObjClass::build_indices()
{
    for (u32 i = 0, n = field_names.size(); i < n; i++)
        field_index_map[field_names[i]] = i;
    for (u32 i = 0, n = method_names.size(); i < n; i++)
        method_slot_map[method_names[i]] = i;
}

int ObjClass::field_index(StringRef field_name) const
{
    u32 const* p = field_index_map.find_ptr(field_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

int ObjClass::method_slot(StringRef method_name) const
{
    u32 const* p = method_slot_map.find_ptr(method_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

ObjInstance::ObjInstance(Array<Value, /*_Alloc=*/GarbageCollector> fields)
    : obj(ObjType::INSTANCE)
    , fields(fields)
{
}

} // namespace fairuz::runtime
