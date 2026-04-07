/// gc.cc

#include "../include/gc.hpp"
#include "../include/vm.hpp"

namespace fairuz::runtime {

void Fa_GarbageCollector::collect(Fa_VM* vm)
{
    mark_roots(vm);
    trace_references();
    sweep();
}

void Fa_GarbageCollector::mark_roots(Fa_VM* vm)
{
    // Stack values
    for (int i = 0; i < vm->m_stack_top && i < Fa_VM::STACK_SIZE; i += 1) {
        if (Fa_IS_OBJECT(vm->m_stack[i]))
            mark_object(Fa_AS_OBJECT(vm->m_stack[i]));
    }

    // Call frames
    for (int i = 0; i < vm->m_frames_top && i < Fa_VM::MAX_FRAMES; i += 1)
        mark_object(vm->m_frames[i].closure);

    // Global slots are the live Fa_VM roots for globals.
    mark_value_array(vm->m_global_slots);
}

void Fa_GarbageCollector::mark_object(Fa_ObjHeader* p)
{
    if (p == nullptr || p->is_marked)
        return;

    p->is_marked = true;
    m_grays.push(p);
}

void Fa_GarbageCollector::blacken_object(Fa_ObjHeader* obj)
{
    switch (obj->type) {
    case Fa_ObjType::CLOSURE: {
        auto cl = static_cast<Fa_ObjClosure*>(obj);
        mark_object(cl->function);
    } break;
    case Fa_ObjType::FUNCTION: {
        mark_object(static_cast<Fa_ObjFunction*>(obj)->name);
        auto constants = &static_cast<Fa_ObjFunction*>(obj)->chunk->constants;
        mark_value_array(*constants);
    } break;
    case Fa_ObjType::LIST: {
        auto lst = static_cast<Fa_ObjList*>(obj);
        mark_value_array(lst->elements);
    } break;
    case Fa_ObjType::DICT: {
        /// TODO:
    } break;
    case Fa_ObjType::NATIVE: // natives are static
    case Fa_ObjType::STRING: // strings are managed with arena
        break;
    }
}

void Fa_GarbageCollector::sweep()
{
    u32 i = 0;
    while (i < m_all.size()) {
        if (!m_all[i]->is_marked) {
            delete m_all[i];
            m_all.erase(i);
            // delay decrement until erase succeeds
            m_current_size -= sizeof(Fa_ObjHeader);
        } else {
            m_all[i]->is_marked = false;
            i += 1;
        }
    }
}

void Fa_GarbageCollector::mark_value_array(Fa_Array<Fa_Value> const& arr)
{
    for (u32 i = 0, n = arr.size(); i < n; i += 1) {
        if (Fa_IS_OBJECT(arr[i]))
            mark_object(Fa_AS_OBJECT(arr[i]));
    }
}

void Fa_GarbageCollector::trace_references()
{
    while (!m_grays.empty()) {
        Fa_ObjHeader* obj = m_grays.back();
        m_grays.pop();
        blacken_object(obj);
    }
}

// gc.cc
void Fa_GarbageCollector::sweep_all()
{
    for (u32 i = 0; i < m_all.size(); i += 1)
        delete m_all[i];

    m_all.clear();
    m_current_size = 0;
}

} // namespace fairuz::runtime
