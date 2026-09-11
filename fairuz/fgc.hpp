#ifndef FA_GC_HPP
#define FA_GC_HPP

#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "fobject.hpp"
#include "fvalue.hpp"

#include <new>

namespace fairuz::runtime {

class Fa_VM;
using StringArr = Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector>;

class Fa_GarbageCollector {
private:
    Fa_Set<Fa_ObjHeader*> m_all;   // all tracked objects list
    Fa_Set<Fa_ObjHeader*> m_grays; // all gray objects list
    u64 m_current_size { 0 };      // current tracked memory in bytes
    u64 m_next_collection { 4096 };

public:
    Fa_GarbageCollector() = default;

    ~Fa_GarbageCollector()
    {
        sweep_all();
    }

    void collect(Fa_VM* vm);
    bool should_collect() const { return m_current_size >= m_next_collection; }

    template<typename T, typename... Args>
    T* make(Args&&... m_args)
    {
        T* obj = new T(std::forward<Args>(m_args)...);
        if (obj == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        m_all.push(&obj->obj);
        m_current_size += sizeof(T); // reasonable estimate
        return obj;
    }

    u64 current_memory() const { return m_current_size; }
    void sweep_all();

    /* --- obj factory --- */

    Fa_ObjString* make_obj_string(Fa_StringRef str);
    Fa_ObjString* make_obj_string(char const* str);
    Fa_ObjString* make_obj_string(char* str);
    Fa_ObjList* make_obj_list();
    Fa_ObjDict* make_obj_dict(Fa_DictType data = { });
    Fa_ObjFunction* make_obj_function(Fa_Chunk* chunk);
    Fa_ObjNative* make_obj_native(NativeFn fn, Fa_ObjString* name, int arity);
    Fa_ObjClass* make_obj_class(
        Fa_StringRef name,
        StringArr& fields,
        StringArr& methods,
        Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vtable);
    Fa_ObjInstance* make_obj_instance(Fa_ObjClass* klass);
    Fa_ObjFileHandle* make_obj_file_handle(FILE* fp);

    /* --- allocator api --- */
    template<typename T>
    T* allocate_array(u32 const count)
    {
        return static_cast<T*>(allocate(count * sizeof(T)));
    }

    void* allocate(size_t const size)
    {
        void* mem = ::operator new(size, std::nothrow);
        if (mem == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        m_current_size += size;
        return mem;
    }

    void deallocate(void* ptr, size_t const size)
    {
        ::operator delete(ptr);
        m_current_size = size > m_current_size ? 0 : m_current_size - size;
    }

    /* --- Fa_Value constructors --- */

    Fa_Value make_string(Fa_StringRef str) { return Fa_Value::from_string(make_obj_string(str)); }
    Fa_Value make_string(char const* str) { return Fa_Value::from_string(make_obj_string(str)); }
    Fa_Value make_string(char* str) { return Fa_Value::from_string(make_obj_string(str)); }
    Fa_Value make_list() { return Fa_Value::from_list(make_obj_list()); }
    Fa_Value make_dict(Fa_DictType data = { }) { return Fa_Value::from_dict(make_obj_dict(data)); }
    Fa_Value make_function(Fa_Chunk* chunk) { return Fa_Value::from_func(make_obj_function(chunk)); }
    Fa_Value make_native(NativeFn fn, Fa_ObjString* name, int arity) { return Fa_Value::from_native(make_obj_native(fn, name, arity)); }
    Fa_Value make_instance(Fa_ObjClass* klass) { return Fa_Value::from_instance(make_obj_instance(klass)); }
    Fa_Value make_file_handle(FILE* fp) { return Fa_Value::from_file_handle(make_obj_file_handle(fp)); }
    Fa_Value make_class(Fa_StringRef name, StringArr fields, StringArr methods,
        Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vtable)
    {
        return Fa_Value::from_class(make_obj_class(name, fields, methods, vtable));
    }

private:
    void mark_roots(Fa_VM* vm);
    void mark_object(Fa_ObjHeader* p);
    void mark_chunk_constants(Fa_Chunk* chunk);
    void blacken_object(Fa_ObjHeader* obj);
    void sweep();

    void mark_value_array(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> const& arr)
    {
        for (u32 i = 0, n = arr.size(); i < n; i++) {
            if (arr[i].is_obj())
                mark_object(arr[i].as_obj());
        }
    }

    void mark_value_array(Fa_Array<Fa_Value> const& arr)
    {
        for (u32 i = 0, n = arr.size(); i < n; i++) {
            if (arr[i].is_obj())
                mark_object(arr[i].as_obj());
        }
    }

    void trace_references()
    {
        while (!m_grays.empty()) {
            Fa_ObjHeader* obj = m_grays.back();
            m_grays.pop();
            blacken_object(obj);
        }
    }

}; // class Fa_GarbageCollector

} // namespace fairuz::runtime

#endif // FA_GC_HPP
