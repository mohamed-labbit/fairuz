#include "../../../include/runtime/vm/vm.hpp"

namespace mylang {
namespace runtime {

template<typename T, typename... Args>
T* VM::alloc_obj(Args&&... args)
{
    if (gc_allocated_ > gc_threshold_)
        collect_garbage();

    T* obj = new T(std::forward<Args>(args)...);
    gc_allocated_ += sizeof(T);

    // Prepend to intrusive list
    obj->next = gc_head_;
    gc_head_ = obj;
    return obj;
}

void VM::mark_value(Value v)
{
    if (v.isObj())
        mark_object(v.asObj());
}

void VM::mark_object(ObjHeader* obj)
{
    if (!obj || obj->is_marked)
        return;

    obj->is_marked = true;

    switch (obj->type) {
    case ObjType::STRING:
        break; // no references
    case ObjType::LIST:
        for (Value& v : static_cast<ObjList*>(obj)->elements)
            mark_value(v);
        break;
    case ObjType::FUNCTION:
        // Mark constants in the chunk
        if (static_cast<ObjFunction*>(obj)->chunk)
            for (Value& v : static_cast<ObjFunction*>(obj)->chunk->constants)
                mark_value(v);
        break;
    case ObjType::CLOSURE: {
        ObjClosure* cl = static_cast<ObjClosure*>(obj);
        mark_object(cl->function);
        for (ObjUpvalue* uv : cl->upvalues)
            mark_object(uv);

    } break;
    case ObjType::NATIVE:
        mark_object(static_cast<ObjNative*>(obj)->name);
        break;
    case ObjType::UPVALUE: {
        ObjUpvalue* uv = static_cast<ObjUpvalue*>(obj);
        mark_value(*uv->location);
        mark_value(uv->closed);
        break;
    }
    }
}

void VM::mark_roots()
{
    // Stack
    for (int i = 0; i < stack_top_; ++i)
        mark_value(stack_[i]);
    // Frames (closures)
    for (auto& f : frames_)
        mark_object(f.closure);
    // Globals
    for (auto& [k, v] : globals_)
        mark_value(v);
    // Open upvalues
    for (ObjUpvalue* uv : open_upvalues_)
        mark_object(uv);
}

void VM::trace_references()
{
    // We already marked everything reachable in mark_roots() recursively.
    // Nothing further needed for a simple stop-the-world collector.
}

void VM::sweep()
{
    ObjHeader** prev = &gc_head_;
    ObjHeader* cur = gc_head_;

    while (cur) {
        if (cur->is_marked) {
            cur->is_marked = false; // reset for next cycle
            prev = &cur->next;
            cur = cur->next;
        } else {
            ObjHeader* unreachable = cur;
            cur = cur->next;
            *prev = cur;
            gc_allocated_ -= sizeof(*unreachable); // approximate

            // Also remove from string table if it's a string
            if (unreachable->type == ObjType::STRING) {
                auto* s = static_cast<ObjString*>(unreachable);
                string_table_.erase(s->chars);
            }

            switch (unreachable->type) {
            case ObjType::STRING: delete static_cast<ObjString*>(unreachable); break;
            case ObjType::LIST: delete static_cast<ObjList*>(unreachable); break;
            case ObjType::FUNCTION: delete static_cast<ObjFunction*>(unreachable); break;
            case ObjType::CLOSURE: delete static_cast<ObjClosure*>(unreachable); break;
            case ObjType::NATIVE: delete static_cast<ObjNative*>(unreachable); break;
            case ObjType::UPVALUE: delete static_cast<ObjUpvalue*>(unreachable); break;
            }
        }
    }
}

void VM::collect_garbage()
{
    mark_roots();
    trace_references();
    sweep();

    gc_threshold_ = static_cast<size_t>(
        static_cast<double>(gc_allocated_) * GC_GROWTH);

    if (gc_threshold_ < GC_INITIAL)
        gc_threshold_ = GC_INITIAL;
}

}
}
