//
// fgc.cc
//

#include "fgc.hpp"
#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "fstring.hpp"
#include "fvalue.hpp"
#include "fvm.hpp"

#include <algorithm>
#include <new>

namespace fairuz::runtime {

static void fa_delete_object(ObjHeader* obj)
{
    switch (obj->type) {
    case ObjType::STRING: delete obj_cast<ObjString>(obj, ObjType::STRING); break;
    case ObjType::LIST: delete obj_cast<ObjList>(obj, ObjType::LIST); break;
    case ObjType::DICT: delete obj_cast<ObjDict>(obj, ObjType::DICT); break;
    case ObjType::FUNCTION: delete obj_cast<ObjFunction>(obj, ObjType::FUNCTION); break;
    case ObjType::NATIVE: delete obj_cast<ObjNative>(obj, ObjType::NATIVE); break;
    case ObjType::CLASS: delete obj_cast<ObjClass>(obj, ObjType::CLASS); break;
    case ObjType::INSTANCE: delete obj_cast<ObjInstance>(obj, ObjType::INSTANCE); break;
    case ObjType::FILE_HANDLE: delete obj_cast<ObjFileHandle>(obj, ObjType::FILE_HANDLE); break;
    case ObjType::MODULE: delete obj_cast<ObjModule>(obj, ObjType::MODULE); break;
#if FA_USE_NANBOX
    case ObjType::INT: // TODO:
#endif
    case ObjType::_COUNT: diagnostic::panic(ErrorCode::TYPE_ERROR_CALL, "attempting to delete an unknown type"); break; /// unreachable break
    }
}

static size_t fa_object_size(ObjHeader const* obj)
{
    switch (obj->type) {
    case ObjType::STRING: return sizeof(ObjString);
    case ObjType::LIST: return sizeof(ObjList);
    case ObjType::DICT: return sizeof(ObjDict);
    case ObjType::FUNCTION: return sizeof(ObjFunction);
    case ObjType::NATIVE: return sizeof(ObjNative);
    case ObjType::CLASS: return sizeof(ObjClass);
    case ObjType::INSTANCE: return sizeof(ObjInstance);
    case ObjType::FILE_HANDLE: return sizeof(ObjFileHandle);
    case ObjType::MODULE: return sizeof(ObjModule);
#if FA_USE_NANBOX
    case ObjType::INT: return 0;
#endif
    case ObjType::_COUNT: return 0;
    }
    return 0;
}

void GarbageCollector::collect(VM* vm)
{
    mark_roots(vm);
    trace_references();
    sweep();
    m_next_collection = std::max<u64>(4096, m_current_size * 2);
}

void GarbageCollector::mark_roots(VM* vm)
{
    for (int i = 0; i < vm->m_stack_top && i < VM::STACK_SIZE; i++) {
        if (vm->m_stack[i].is_obj())
            mark_object(vm->m_stack[i].as_obj());
    }

    for (int i = 0; i < vm->m_frames_top && i < VM::MAX_FRAMES; i++) {
        if (vm->m_frames[i].func != nullptr)
            mark_object(&vm->m_frames[i].func->obj);
    }

    mark_value_array(vm->m_builtin_environment.slots);
    mark_value_array(vm->m_root_environment.slots);
    for (auto const& env : vm->m_module_environments) {
        if (env != nullptr)
            mark_value_array(env->slots);
    }
    for (auto const& [path, module] : vm->m_module_cache) {
        (void)path;
        if (module != nullptr)
            mark_object(&module->obj);
    }

    // Interned strings are deliberately strong roots. This trades bounded
    // per-VM interning retention for pointer stability and avoids returning
    // dangling raw pointers after a collection.
    for (auto [name, string] : vm->m_string_table) {
        (void)name;
        if (string != nullptr)
            mark_object(&string->obj);
    }
}

void GarbageCollector::mark_object(ObjHeader* p)
{
    if (p == nullptr || p->is_marked)
        return;

    p->is_marked = true;
    m_grays.push(p);
}

void GarbageCollector::mark_chunk_constants(Chunk* chunk)
{
    if (chunk == nullptr)
        return;

    mark_value_array(chunk->constants);

    for (auto* fn : chunk->functions)
        mark_chunk_constants(fn);
}

void GarbageCollector::blacken_object(ObjHeader* obj)
{
    switch (obj->type) {
    case ObjType::FUNCTION: {
        ObjFunction* fn = obj_cast<ObjFunction>(obj, ObjType::FUNCTION);
        if (fn->chunk != nullptr)
            mark_chunk_constants(fn->chunk);
        break;
    }
    case ObjType::NATIVE: {
        ObjNative* native = obj_cast<ObjNative>(obj, ObjType::NATIVE);
        if (native->name != nullptr)
            mark_object(&native->name->obj);
        break;
    }
    case ObjType::CLASS: {
        ObjClass* klass = obj_cast<ObjClass>(obj, ObjType::CLASS);
        if (klass->parent != nullptr)
            mark_object(&klass->parent->obj);
        for (u32 i = 0, n = klass->vtable.size(); i < n; i++) {
            if (klass->vtable[i] != nullptr)
                mark_chunk_constants(klass->vtable[i]);
        }
        break;
    }
    case ObjType::INSTANCE: {
        ObjInstance* inst = obj_cast<ObjInstance>(obj, ObjType::INSTANCE);
        mark_object(&inst->klass->obj);
        mark_value_array(inst->fields);
        break;
    }
    case ObjType::LIST: {
        ObjList* list = obj_cast<ObjList>(obj, ObjType::LIST);
        mark_value_array(list->elements);
        break;
    }
    case ObjType::DICT: {
        ObjDict* dict = obj_cast<ObjDict>(obj, ObjType::DICT);
        for (auto [k, v] : dict->data) {
            if (k.is_obj())
                mark_object(k.as_obj());
            if (v.is_obj())
                mark_object(v.as_obj());
        }
        break;
    }
    case ObjType::FILE_HANDLE: break;
    case ObjType::MODULE: {
        ObjModule* module = obj_cast<ObjModule>(obj, ObjType::MODULE);
        if (module->globals != nullptr)
            mark_value_array(module->globals->slots);
        if (module->chunk != nullptr)
            mark_chunk_constants(module->chunk);
        break;
    }
    case ObjType::STRING: break;
#if FA_USE_NANBOX
    case ObjType::INT: // TODO:
#endif
    case ObjType::_COUNT: diagnostic::panic(ErrorCode::TYPE_ERROR_CALL, "attempting to blacken an unknown object type");
    }
}

void GarbageCollector::sweep()
{
    u32 i = 0;
    while (i < m_all.size()) {
        ObjHeader* obj = m_all[i];
        if (!obj->is_marked) {
            size_t object_size = fa_object_size(obj);
            fa_delete_object(obj);
            m_current_size = object_size > m_current_size ? 0 : m_current_size - object_size;
            m_all.erase(i);
        } else {
            obj->is_marked = false;
            i++;
        }
    }
}

void GarbageCollector::sweep_all()
{
    for (auto* object : m_all) {
        if (object != nullptr)
            fa_delete_object(object);
    }

    m_all.clear();
    m_grays.clear();
    m_current_size = 0;
}

ObjString* GarbageCollector::make_obj_string(StringRef str)
{
    auto ret = make<ObjString>();
    ret->str = str;
    ret->hash = StringRefHash()(ret->str);
    return ret;
}

ObjString* GarbageCollector::make_obj_string(char const* str)
{
    auto ret = make<ObjString>();
    ret->str = str;
    ret->hash = StringRefHash()(ret->str);
    return ret;
}

ObjString* GarbageCollector::make_obj_string(char* str)
{
    return make_obj_string(static_cast<char const*>(str));
}

ObjList* GarbageCollector::make_obj_list()
{
    void* mem = ::operator new(sizeof(ObjList), std::nothrow);
    if (mem == nullptr)
        diagnostic::panic(ErrorCode::ALLOC_FAILED);

    auto elems = Array<Value, /*_Alloc=*/GarbageCollector> { this };

    ObjList* ret = new (mem) ObjList(elems);

    m_all.push(&ret->obj);
    m_current_size += sizeof(ObjList);
    return ret;
}

ObjDict* GarbageCollector::make_obj_dict(DictType data)
{
    auto ret = make<ObjDict>();
    ret->data = std::move(data);
    for (auto [key, value] : ret->data) {
        (void)value;
        ret->insertion_order.push(key);
    }
    return ret;
}

ObjFunction* GarbageCollector::make_obj_function(Chunk* chunk, GlobalEnvironment* globals)
{
    auto ret = make<ObjFunction>();
    ret->chunk = chunk;
    ret->globals = globals;
    return ret;
}

ObjModule* GarbageCollector::make_obj_module(std::string name, std::string path, GlobalEnvironment* globals)
{
    auto ret = make<ObjModule>();
    ret->name = std::move(name);
    ret->path = std::move(path);
    ret->globals = globals;
    return ret;
}

ObjNative* GarbageCollector::make_obj_native(NativeFn fn, ObjString* name, int arity)
{
    if (name == nullptr || fn == nullptr) {
        /// NOTE: this should never happen in production
        /// if it was ever detected, it must be patched right away
        diagnostic::panic(ErrorCode::INVALID_PARAMETER);
        return nullptr;
    }

    auto ret = make<ObjNative>();
    ret->arity = arity;
    ret->name = name;
    ret->fn = fn;
    return ret;
}

ObjClass* GarbageCollector::make_obj_class(
    StringRef name,
    StringArr& fields,
    StringArr& methods,
    Array<Chunk*, /*_Alloc=*/GarbageCollector> vtable)
{
    auto ret = make<ObjClass>(fields, methods, vtable);
    ret->name = name;
    ret->build_indices();

    return ret;
}

ObjInstance* GarbageCollector::make_obj_instance(ObjClass* klass)
{
    assert(klass != nullptr && "instance must be constructed with a valid class");

    Array<Value, /*_Alloc=*/GarbageCollector> fields {
        klass->field_names.size(), Value::nil(), this
    };
    auto ret = make<ObjInstance>(fields);
    ret->klass = klass;

    return ret;
}

ObjFileHandle* GarbageCollector::make_obj_file_handle(FILE* fp)
{
    assert(fp != nullptr && "file handle object must be constructed with a valid file pointer");

    auto ret = make<ObjFileHandle>();
    ret->fp = fp;
    ret->is_open = true;

    return ret;
}

} // namespace fairuz::runtime
