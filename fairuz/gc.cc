#include "gc.hpp"
#include "diagnostic.hpp"
#include "macros.hpp"
#include "value.hpp"
#include "vm.hpp"

#include <new>

namespace fairuz::runtime {

static void fa_delete_object(Fa_ObjHeader* obj)
{
    switch (obj->type) {
    case Fa_ObjType::STRING: delete Fa_obj_cast<Fa_ObjString>(obj, Fa_ObjType::STRING); break;
    case Fa_ObjType::LIST: delete Fa_obj_cast<Fa_ObjList>(obj, Fa_ObjType::LIST); break;
    case Fa_ObjType::DICT: delete Fa_obj_cast<Fa_ObjDict>(obj, Fa_ObjType::DICT); break;
    case Fa_ObjType::FUNCTION: delete Fa_obj_cast<Fa_ObjFunction>(obj, Fa_ObjType::FUNCTION); break;
    case Fa_ObjType::NATIVE: delete Fa_obj_cast<Fa_ObjNative>(obj, Fa_ObjType::NATIVE); break;
    case Fa_ObjType::CLASS: delete Fa_obj_cast<Fa_ObjClass>(obj, Fa_ObjType::CLASS); break;
    case Fa_ObjType::INSTANCE: delete Fa_obj_cast<Fa_ObjInstance>(obj, Fa_ObjType::INSTANCE); break;
    case Fa_ObjType::_COUNT: diagnostic::panic(ErrorCode::TYPE_ERROR_CALL); break; /// unreachable break
    }
}

void Fa_GarbageCollector::collect(Fa_VM* vm)
{
    mark_roots(vm);
    trace_references();
    sweep();
}

void Fa_GarbageCollector::mark_roots(Fa_VM* vm)
{
    for (int i = 0; i < vm->m_stack_top && i < Fa_VM::STACK_SIZE; i += 1) {
        if (Fa_IS_OBJECT(vm->m_stack[i]))
            mark_object(Fa_AS_OBJECT(vm->m_stack[i]));
    }

    for (int i = 0; i < vm->m_frames_top && i < Fa_VM::MAX_FRAMES; i += 1) {
        if (vm->m_frames[i].func != nullptr)
            mark_object(&vm->m_frames[i].func->obj);
    }

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
    case Fa_ObjType::FUNCTION: {
        Fa_ObjFunction* fn = Fa_obj_cast<Fa_ObjFunction>(obj, Fa_ObjType::FUNCTION);
        if (fn->chunk != nullptr)
            mark_value_array(fn->chunk->constants);
        break;
    }
    case Fa_ObjType::NATIVE: {
        Fa_ObjNative* native = Fa_obj_cast<Fa_ObjNative>(obj, Fa_ObjType::NATIVE);
        if (native->name != nullptr)
            mark_object(&native->name->obj);
        break;
    }
    case Fa_ObjType::CLASS: {
        Fa_ObjClass* klass = Fa_obj_cast<Fa_ObjClass>(obj, Fa_ObjType::CLASS);
        for (u32 i = 0, n = klass->vtable_size; i < n; i += 1) {
            if (klass->vtable[i] != nullptr)
                mark_value_array(klass->vtable[i]->constants);
        }
        break;
    }
    case Fa_ObjType::INSTANCE: {
        Fa_ObjInstance* inst = Fa_obj_cast<Fa_ObjInstance>(obj, Fa_ObjType::INSTANCE);
        mark_object(&inst->klass->obj);
        for (u32 i = 0, n = inst->field_count; i < n; i += 1) {
            if (Fa_IS_OBJECT(inst->fields[i]))
                mark_object(Fa_AS_OBJECT(inst->fields[i]));
        }
        break;
    }
    case Fa_ObjType::LIST: {
        Fa_ObjList* list = Fa_obj_cast<Fa_ObjList>(obj, Fa_ObjType::LIST);
        mark_value_array(list->elements);
        break;
    }
    case Fa_ObjType::DICT: {
        Fa_ObjDict* dict = Fa_obj_cast<Fa_ObjDict>(obj, Fa_ObjType::DICT);
        for (auto [k, v] : dict->data) {
            if (Fa_IS_OBJECT(k))
                mark_object(Fa_AS_OBJECT(k));
            if (Fa_IS_OBJECT(v))
                mark_object(Fa_AS_OBJECT(v));
        }
        break;
    }
    case Fa_ObjType::STRING:
        break;
    case Fa_ObjType::_COUNT:
        diagnostic::panic(ErrorCode::TYPE_ERROR_CALL);
    }
}

void Fa_GarbageCollector::sweep()
{
    u32 i = 0;
    while (i < m_all.size()) {
        Fa_ObjHeader* obj = m_all[i];
        if (!obj->is_marked) {
            fa_delete_object(obj);
            m_all.erase(i);
        } else {
            obj->is_marked = false;
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

void Fa_GarbageCollector::sweep_all()
{
    for (u32 i = 0; i < m_all.size(); i += 1)
        fa_delete_object(m_all[i]);

    m_all.clear();
    m_grays.clear();
    m_current_size = 0;
}

Fa_ObjString* Fa_GarbageCollector::make_obj_string(Fa_RTStringRef str)
{
    return make<Fa_ObjString>(str);
}

Fa_ObjString* Fa_GarbageCollector::make_obj_string(Fa_StringRef str) {
    return make<Fa_ObjString>(str.data());
}

Fa_ObjString* Fa_GarbageCollector::make_obj_string(char const* str)
{
    return make<Fa_ObjString>(str);
}

Fa_ObjString* Fa_GarbageCollector::make_obj_string(char* str)
{
    return make_obj_string(static_cast<char const*>(str));
}

Fa_ObjList* Fa_GarbageCollector::make_obj_list()
{
    return make<Fa_ObjList>();
}

Fa_ObjDict* Fa_GarbageCollector::make_obj_dict(Fa_DictType data)
{
    return make<Fa_ObjDict>(std::move(data));
}

Fa_ObjFunction* Fa_GarbageCollector::make_obj_function(Fa_Chunk* chunk)
{
    return make<Fa_ObjFunction>(chunk);
}

Fa_ObjNative* Fa_GarbageCollector::make_obj_native(NativeFn fn, Fa_ObjString* name, int arity)
{
    return make<Fa_ObjNative>(fn, name, arity);
}

Fa_ObjClass* Fa_GarbageCollector::make_obj_class(Fa_RTStringRef name, Fa_RTStringRef* fields, 
    u32 field_count, Fa_RTStringRef* methods, u32 method_count, Fa_Chunk** vtable, u32 vtable_size)
{
    return make<Fa_ObjClass>(name, fields, field_count, methods, method_count, vtable, vtable_size);
}

Fa_ObjInstance* Fa_GarbageCollector::make_obj_instance(Fa_ObjClass* klass)
{
    assert(klass != nullptr && "instance must be constructed with a valid class");

    Fa_Value* fields = nullptr;
    if (klass->field_count > 0) {
        fields = new (std::nothrow) Fa_Value[klass->field_count];
        if (fields == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        for (u32 i = 0; i < klass->field_count; i += 1)
            fields[i] = NIL_VAL;
    }

    Fa_ObjInstance* obj = new (std::nothrow) Fa_ObjInstance(klass, fields, klass->field_count);
    if (obj == nullptr) {
        delete[] fields;
        diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
    }

    m_all.push(&obj->obj);
    m_current_size += sizeof(Fa_ObjInstance) + sizeof(Fa_Value) * obj->field_count;
    return obj;
}

} // namespace fairuz::runtime
