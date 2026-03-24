/// gc.cc

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
    for (int i = 0; i < vm->StackTop_ && i < VM::STACK_SIZE; ++i) {
        if (IS_OBJECT(vm->Stack_[i]))
            markObject(AS_OBJECT(vm->Stack_[i]));
    }

    // Call frames
    for (int i = 0; i < vm->FramesTop_ && i < VM::MAX_FRAMES; ++i)
        markObject(vm->Frames_[i].closure);

    // Open upvalues
    for (uint32_t i = 0, n = vm->OpenUpvalues_.size(); i < n; ++i)
        markObject(vm->OpenUpvalues_[i]);

    // Global slots are the live VM roots for globals.
    markValueArray(vm->GlobalSlots_);
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
            // delay decrement until erase succeeds
            CurrentSize_ -= sizeof(ObjHeader);
        } else {
            All_[i]->isMarked = false;
            ++i;
        }
    }
}

void GarbageCollector::markValueArray(Array<Value> const& arr)
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

// gc.cc
void GarbageCollector::sweepAll()
{
    for (uint32_t i = 0; i < All_.size(); ++i)
        delete All_[i];

    All_.clear();
    CurrentSize_ = 0;
}

} // namespace mylang::runtime
