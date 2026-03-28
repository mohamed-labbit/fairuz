#ifndef GC_HPP
#define GC_HPP

#include "array.hpp"
#include "value.hpp"

namespace fairuz::runtime {

class Fa_VM;

class Fa_GarbageCollector {
private:
    Fa_Array<Fa_ObjHeader*> All_;
    Fa_Array<Fa_ObjHeader*> Grays_;

    u32 CurrentSize_ { 0 };

public:
    Fa_GarbageCollector() = default;

    void collect(Fa_VM* vm);

    template<typename T, typename... Args>
    T* make(Args&&... args)
    {
        T* obj = new T(std::forward<Args>(args)...);
        All_.push(static_cast<Fa_ObjHeader*>(obj));
        CurrentSize_ += sizeof(T); // reasonable estimate
        return obj;
    }

    u32 currentMemory() const { return CurrentSize_; }
    void sweepAll();

private:
    void markRoots(Fa_VM* vm);
    void markObject(Fa_ObjHeader* p);
    void blackenObject(Fa_ObjHeader* obj);
    void sweep();
    void markValueArray(Fa_Array<Fa_Value> const& arr);
    void traceReferences();
}; // class Fa_GarbageCollector

} // namespace fairuz::runtime

#endif // GC_HPP
