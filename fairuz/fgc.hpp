#ifndef FA_GC_HPP
#define FA_GC_HPP

#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "fvalue.hpp"

#include <new>

namespace fairuz::runtime {

class VM;
using StringArr = Array<StringRef, /*_Alloc=*/GarbageCollector>;

class GarbageCollector {
private:
    Set<ObjHeader*> m_all;    // all tracked objects list
    Set<ObjHeader*> m_grays;  // all gray objects list
    u64 m_current_size { 0 }; // current tracked memory in bytes
    u64 m_next_collection { 4096 };

public:
    GarbageCollector() = default;

    ~GarbageCollector()
    {
        sweep_all();
    }

    void collect(VM* vm);
    bool should_collect() const { return m_current_size >= m_next_collection; }

    template<typename T, typename... Args>
    T* make(Args&&... m_args)
    {
        T* obj = new T(std::forward<Args>(m_args)...);
        if (obj == nullptr)
            diagnostic::panic(ErrorCode::ALLOC_FAILED);
        m_all.push(&obj->obj);
        m_current_size += sizeof(T); // reasonable estimate
        return obj;
    }

    u64 current_memory() const { return m_current_size; }
    void sweep_all();

    /* --- obj factory --- */

    ObjBigInt* make_obj_int(i64 const v);
    ObjBigInt* make_obj_int(integer::Data data);
    ObjString* make_obj_string(StringRef str);
    ObjString* make_obj_string(char const* str);
    ObjString* make_obj_string(char* str);
    ObjList* make_obj_list();
    ObjDict* make_obj_dict(DictType data = { });
    ObjFunction* make_obj_function(Chunk* chunk, GlobalEnvironment* globals = nullptr);
    ObjNative* make_obj_native(NativeFn fn, ObjString* name, int arity);
    ObjClass* make_obj_class(
        StringRef name,
        StringArr& fields,
        StringArr& methods,
        Array<Chunk*, /*_Alloc=*/GarbageCollector> vtable);
    ObjInstance* make_obj_instance(ObjClass* klass);
    ObjFileHandle* make_obj_file_handle(FILE* fp);
    ObjModule* make_obj_module(std::string name, std::string path, GlobalEnvironment* globals);

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
            diagnostic::panic(ErrorCode::ALLOC_FAILED);
        m_current_size += size;
        return mem;
    }

    void deallocate(void* ptr, size_t const size)
    {
        ::operator delete(ptr);
        m_current_size = size > m_current_size ? 0 : m_current_size - size;
    }

    /* --- Value constructors --- */

#if FA_USE_NANBOX
    Value make_int(i64 const v) { return integer::finish(integer::from_i64(v), *this); }
    Value make_int(Array<u32>& limbs, bool sign) { return integer::finish({ integer::Limbs(limbs.begin(), limbs.end()), sign }, *this); }
#endif
    Value make_string(StringRef str) { return Value::from_string(make_obj_string(str)); }
    Value make_string(char const* str) { return Value::from_string(make_obj_string(str)); }
    Value make_string(char* str) { return Value::from_string(make_obj_string(str)); }
    Value make_list() { return Value::from_list(make_obj_list()); }
    Value make_dict(DictType data = { }) { return Value::from_dict(make_obj_dict(data)); }
    Value make_function(Chunk* chunk, GlobalEnvironment* globals = nullptr) { return Value::from_func(make_obj_function(chunk, globals)); }
    Value make_native(NativeFn fn, ObjString* name, int arity) { return Value::from_native(make_obj_native(fn, name, arity)); }
    Value make_instance(ObjClass* klass) { return Value::from_instance(make_obj_instance(klass)); }
    Value make_file_handle(FILE* fp) { return Value::from_file_handle(make_obj_file_handle(fp)); }
    Value make_module(std::string name, std::string path, GlobalEnvironment* globals)
    {
        return Value::from_module(make_obj_module(std::move(name), std::move(path), globals));
    }
    Value make_class(StringRef name, StringArr fields, StringArr methods,
        Array<Chunk*, /*_Alloc=*/GarbageCollector> vtable)
    {
        return Value::from_class(make_obj_class(name, fields, methods, vtable));
    }

private:
    void mark_roots(VM* vm);
    void mark_object(ObjHeader* p);
    void mark_chunk_constants(Chunk* chunk);
    void blacken_object(ObjHeader* obj);
    void sweep();

    void mark_value_array(Array<Value, /*_Alloc=*/GarbageCollector> const& arr)
    {
        for (u32 i = 0, n = arr.size(); i < n; i++) {
            if (arr[i].is_obj())
                mark_object(arr[i].as_obj());
        }
    }

    void mark_value_array(Array<Value> const& arr)
    {
        for (u32 i = 0, n = arr.size(); i < n; i++) {
            if (arr[i].is_obj())
                mark_object(arr[i].as_obj());
        }
    }

    void trace_references()
    {
        while (!m_grays.empty()) {
            ObjHeader* obj = m_grays.back();
            m_grays.pop();
            blacken_object(obj);
        }
    }

}; // class GarbageCollector

} // namespace fairuz::runtime

#endif // FA_GC_HPP
