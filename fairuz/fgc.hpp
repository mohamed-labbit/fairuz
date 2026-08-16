#ifndef GC_HPP
#define GC_HPP

#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "fobject.hpp"
#include "fvalue.hpp"

namespace fairuz::runtime {

class Fa_VM;

class Fa_GarbageCollector {
private:
    Fa_Set<Fa_ObjHeader*> m_all;   // all tracked objects list
    Fa_Set<Fa_ObjHeader*> m_grays; // all gray objects list
    u64 m_current_size { 0 };      // current tracked memory in bytes

public:
    Fa_GarbageCollector() = default;

    void collect(Fa_VM* vm);

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
        Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> fields,
        Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> methods,
        Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> vtable);
    Fa_ObjInstance* make_obj_instance(Fa_ObjClass* klass);
    Fa_ObjFileHandle* make_obj_file_handle(FILE* fp);

    /* --- allocator api --- */
    template<typename T>
    T* allocate_array(u32 const count)
    {
        return static_cast<T*>(allocate(count * sizeof(T)));
    }

    void* allocate(u32 const size)
    {
        void* mem = ::operator new(size, std::nothrow);
        if (mem == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);
        m_current_size += size;
        return mem;
    }

    void deallocate(void* ptr, u32 const size)
    {
        ::operator delete(ptr);
        m_current_size -= size;
    }

private:
    void mark_roots(Fa_VM* vm);
    void mark_object(Fa_ObjHeader* p);
    void mark_chunk_constants(Fa_Chunk* chunk);
    void blacken_object(Fa_ObjHeader* obj);
    void sweep();
    void mark_value_array(Fa_Array<Fa_Value, /*_Alloc=*/Fa_GarbageCollector> const& arr);
    void mark_value_array(Fa_Array<Fa_Value> const& arr);
    void trace_references();
}; // class Fa_GarbageCollector

} // namespace fairuz::runtime

#endif // GC_HPP
