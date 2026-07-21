#pragma once

#include "fobject.hpp"

namespace fairuz::runtime {

// object defs

void Fa_ObjList::reserve(u32 cap) { elements.reserve(cap); }
u32 Fa_ObjList::size() const { return elements.size(); }
void Fa_ObjList::push(Fa_Value& v) { elements.push(v); }
bool Fa_ObjList::empty() const { return elements.empty(); }

Fa_StringRef Fa_ObjFunction::name() const { return chunk->name; }
u32 Fa_ObjFunction::arity() const { return chunk->arity; }

void Fa_ObjClass::build_indices()
{
    for (u32 i = 0, n = field_count; i < n; i += 1)
        field_index_map[field_names[i]] = i;

    for (u32 i = 0, n = method_count; i < n; i += 1)
        method_slot_map[method_names[i]] = i;
}

int Fa_ObjClass::field_index(Fa_RTStringRef field_name) const
{
    u32 const* p = field_index_map.find_ptr(field_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

int Fa_ObjClass::method_slot(Fa_RTStringRef method_name) const
{
    u32 const* p = method_slot_map.find_ptr(method_name);
    return p != nullptr ? static_cast<int>(*p) : -1;
}

} // namespace fairuz::runtime
