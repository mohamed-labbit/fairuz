#include "fgc.hpp"
#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"
#include "fvalue.hpp"
#include "fvm.hpp"

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
        mark_value_array(inst->fields);
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

void Fa_GarbageCollector::mark_value_array(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> const& arr)
{
    for (u32 i = 0, n = arr.size(); i < n; i += 1) {
        if (Fa_IS_OBJECT(arr[i]))
            mark_object(Fa_AS_OBJECT(arr[i]));
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

Fa_ObjString* Fa_GarbageCollector::make_obj_string(Fa_StringRef str)
{
    auto ret = make<Fa_ObjString>();
    ret->str = str;
    ret->hash = Fa_StringRefHash()(ret->str);
    return ret;
}

// Fa_ObjString* Fa_GarbageCollector::make_obj_string(Fa_StringRef str)
// {
// return make<Fa_ObjString>(str.data());
// }

Fa_ObjString* Fa_GarbageCollector::make_obj_string(char const* str)
{
    auto ret = make<Fa_ObjString>();
    ret->str = str;
    ret->hash = Fa_StringRefHash()(ret->str);
    return ret;
}

Fa_ObjString* Fa_GarbageCollector::make_obj_string(char* str)
{
    return make_obj_string(static_cast<char const*>(str));
}

/*
Fa_ObjList* Fa_GarbageCollector::make_obj_list()
{
    auto ret = make<Fa_ObjList>();
    return ret;
}
*/

Fa_ObjList* Fa_GarbageCollector::make_obj_list()
{
    void* mem = ::operator new(sizeof(Fa_ObjList), std::nothrow);
    if (mem == nullptr)
        diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
    
    auto elems = Fa_Array<Fa_Value, /*_Alloc=*/ Fa_GarbageCollector>{this};

    Fa_ObjList* ret = new (mem) Fa_ObjList(elems);

    m_all.push(&ret->obj);
    m_current_size += sizeof(Fa_ObjList);
    return ret;
}

Fa_ObjDict* Fa_GarbageCollector::make_obj_dict(Fa_DictType data)
{
    auto ret = make<Fa_ObjDict>();
    ret->data = std::move(data);
    return ret;
}

Fa_ObjFunction* Fa_GarbageCollector::make_obj_function(Fa_Chunk* chunk)
{
    auto ret = make<Fa_ObjFunction>();
    ret->chunk = chunk;
    return ret;
}

Fa_ObjNative* Fa_GarbageCollector::make_obj_native(NativeFn fn, Fa_ObjString* name, int arity)
{
    if (name == nullptr || fn == nullptr) {
        diagnostic::emit(diagnostic::errc::general::Code::INVALID_PARAMETER);
        return nullptr;
    }

    auto ret = make<Fa_ObjNative>();
    ret->arity = arity;
    ret->name = name;
    ret->fn = fn;
    return ret;    
}

Fa_ObjClass* Fa_GarbageCollector::make_obj_class(Fa_StringRef name, Fa_StringRef* fields,
    u32 field_count, Fa_StringRef* methods, u32 method_count, Fa_Chunk** vtable, u32 vtable_size)
{
    if (fields == nullptr || methods == nullptr || vtable == nullptr)
    {
        diagnostic::emit(diagnostic::errc::general::Code::INVALID_PARAMETER);
        return nullptr;
    }

    auto ret = make<Fa_ObjClass>();
    ret->name = name;
    ret->field_names = fields;
    ret->field_count = field_count;
    ret->method_names = methods;
    ret->method_count = method_count;
    ret->vtable = vtable;
    ret->vtable_size = vtable_size;
    ret->build_indices();

    return ret;
}

Fa_ObjInstance* Fa_GarbageCollector::make_obj_instance(Fa_ObjClass* klass)
{
    assert(klass != nullptr && "instance must be constructed with a valid class");

    Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> fields{klass->field_count, Fa_MAKE_NIL(), this};
    Fa_ObjInstance* obj = make<Fa_ObjInstance>(fields);
    obj->klass = klass;

    return obj;
}

} // namespace fairuz::runtime
