#ifndef GC_HPP
#define GC_HPP

#include "array.hpp"
#include "value.hpp"

namespace fairuz::runtime {

class Fa_VM;

class Fa_GarbageCollector {
private:
    Fa_Array<Fa_ObjHeader*> m_all;
    Fa_Array<Fa_ObjHeader*> m_grays;

    u32 m_current_size { 0 };

public:
    Fa_GarbageCollector() = default;

    void collect(Fa_VM* vm);

    template<typename T, typename... Args>
    T* make(Args&&... m_args)
    {
        T* obj = new T(std::forward<Args>(m_args)...);
        m_all.push(static_cast<Fa_ObjHeader*>(obj));
        m_current_size += sizeof(T); // reasonable estimate
        return obj;
    }

    u32 current_memory() const { return m_current_size; }
    void sweep_all();

private:
    void mark_roots(Fa_VM* vm);
    void mark_object(Fa_ObjHeader* p);
    void blacken_object(Fa_ObjHeader* obj);
    void sweep();
    void mark_value_array(Fa_Array<Fa_Value> const& arr);
    void trace_references();
}; // class Fa_GarbageCollector

} // namespace fairuz::runtime

#endif // GC_HPP
