//
// fobject.cc
//

#include "fobject.hpp"
#include "fgc.hpp"
#include "fopcode.hpp"

namespace fairuz::runtime {

// object defs

Fa_ObjList::Fa_ObjList(Fa_ListType elems)
    : elements(std::move(elems))
{
    obj = Fa_ObjHeader { Fa_ObjType::LIST };
}
void Fa_ObjList::reserve(u32 cap) { elements.reserve(cap); }
u32 Fa_ObjList::size() const { return elements.size(); }
void Fa_ObjList::push(Fa_Value& v) { elements.push(v); }
bool Fa_ObjList::empty() const { return elements.empty(); }

Fa_StringRef Fa_ObjFunction::name() const { return chunk->name; }
u32 Fa_ObjFunction::arity() const { return chunk->arity; }

Fa_ObjClass::Fa_ObjClass(
    Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> f,
    Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> m,
    Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vt)
    : field_names(f)
    , method_names(m)
    , vtable(vt)
{
}

void Fa_ObjClass::build_indices()
{
    for (u32 i = 0, n = field_names.size(); i < n; i += 1)
        field_index_map[field_names[i]] = i;
    for (u32 i = 0, n = method_names.size(); i < n; i += 1)
        method_slot_map[method_names[i]] = i;
}

int Fa_ObjClass::field_index(Fa_StringRef field_name) const
{
    u32 const* p = field_index_map.find_ptr(field_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

int Fa_ObjClass::method_slot(Fa_StringRef method_name) const
{
    u32 const* p = method_slot_map.find_ptr(method_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

Fa_ObjInstance::Fa_ObjInstance(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields)
    : obj(Fa_ObjType::INSTANCE)
    , fields(fields)
{
}

} // namespace fairuz::runtime
