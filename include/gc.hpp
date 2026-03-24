#ifndef GC_HPP
#define GC_HPP

#include "array.hpp"
#include "value.hpp"

namespace mylang::runtime {

class VM;

class GarbageCollector {
private:
    Array<ObjHeader*> All_;
    Array<ObjHeader*> Grays_;

    uint32_t CurrentSize_ { 0 };

public:
    GarbageCollector() = default;

    void collect(VM* vm);

    template<typename T, typename... Args>
    T* make(Args&&... args)
    {
        T* obj = new T(std::forward<Args>(args)...);
        All_.push(static_cast<ObjHeader*>(obj));
        CurrentSize_ += sizeof(T); // reasonable estimate
        return obj;
    }

    uint32_t currentMemory() const { return CurrentSize_; }
    void sweepAll();

private:
    void markRoots(VM* vm);
    void markObject(ObjHeader* p);
    void blackenObject(ObjHeader* obj);
    void sweep();
    void markValueArray(Array<Value> const& arr);
    void traceReferences();
};

} // namespace mylang::runtime

#endif // GC_HPP
