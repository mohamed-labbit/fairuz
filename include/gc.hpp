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
public:
    GarbageCollector() = default;

    void collect(VM* vm);

    template<typename T, typename... Args>
    T* make(Args&&... args)
    {
        T* obj = new T(std::forward<Args>(args)...);
        All_.push(static_cast<ObjHeader*>(obj));
        /// TODO: threshold??
        return obj;
    }

private:
    void markRoots(VM* vm);
    void markObject(ObjHeader* p);
    void blackenObject(ObjHeader* obj);
    void sweep();
    void markValueArray(const Array<Value>& arr);
    void traceReferences();
};

} // namespace mylang::runtime

#endif // GC_HPP
