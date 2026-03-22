#include "../include/gc.hpp"
#include "../include/vm.hpp"

namespace mylang::runtime {
    
void GarbageCollector::collect(VM* vm)
{
    markRoots(vm);
    traceReferences();
    sweep();
}

void GarbageCollector::markRoots(VM* vm)
{
    // Stack values
    for (uint32_t i = 0; i < vm->StackTop_; ++i) {
        if (IS_OBJECT(vm->Stack_[i]))
            markObject(AS_OBJECT(vm->Stack_[i]));
    }

    // Call frames
    for (uint32_t i = 0; i < vm->FramesTop_; ++i)
        markObject(vm->Frames_[i].closure);

    // Open upvalues
    for (uint32_t i = 0, n = vm->OpenUpvalues_.size(); i < n; ++i)
        markObject(vm->OpenUpvalues_[i]);

    // Globals
    for (auto& [key, val] : vm->Globals_) {
        if (IS_OBJECT(val))
            markObject(AS_OBJECT(val));
    }
}

void GarbageCollector::markObject(ObjHeader* p)
{
    if (!p || p->isMarked)
        return;

    p->isMarked = true;
    Grays_.push(p);
}

void GarbageCollector::blackenObject(ObjHeader* obj)
{
    switch (obj->type) {
    case ObjType::CLOSURE: {
        auto cl = static_cast<ObjClosure*>(obj);
        markObject(cl->function);
        for (uint32_t i = 0, n = cl->upValues.size(); i < n; ++i)
            markObject(cl->upValues[i]);
    } break;
    case ObjType::FUNCTION: {
        markObject(static_cast<ObjFunction*>(obj)->name);
        auto constants = &static_cast<ObjFunction*>(obj)->chunk->constants;
        markValueArray(*constants);
    } break;
    case ObjType::LIST: {
        auto lst = static_cast<ObjList*>(obj);
        markValueArray(lst->elements);
    } break;
    case ObjType::NATIVE: // natives are static
    case ObjType::STRING: // strings are managed with arena
        break;
    case ObjType::UPVALUE: {
        auto* uv = static_cast<ObjUpvalue*>(obj);
        if (uv->location == &uv->closed) {
            if (IS_OBJECT(uv->closed))
                markObject(AS_OBJECT(uv->closed));
        }
    } break;
    }
}

void GarbageCollector::sweep()
{
    uint32_t i = 0;
    while (i < All_.size()) {
        if (!All_[i]->isMarked) {
            delete All_[i];
            All_.erase(i);
        } else {
            All_[i]->isMarked = false;
            ++i;
        }
    }
}

void GarbageCollector::markValueArray(const Array<Value>& arr)
{
    for (uint32_t i = 0, n = arr.size(); i < n; ++i) {
        if (IS_OBJECT(arr[i]))
            markObject(AS_OBJECT(arr[i]));
    }
}

void GarbageCollector::traceReferences()
{
    while (!Grays_.empty()) {
        ObjHeader* obj = Grays_.back();
        Grays_.pop();
        blackenObject(obj);
    }
}
    
} // namespace mylang::runtime 