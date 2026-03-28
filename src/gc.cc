/// gc.cc

#include "../include/gc.hpp"
#include "../include/vm.hpp"

namespace fairuz::runtime {

void Fa_GarbageCollector::collect(Fa_VM* vm)
{
    markRoots(vm);
    traceReferences();
    sweep();
}

void Fa_GarbageCollector::markRoots(Fa_VM* vm)
{
    // Stack values
    for (int i = 0; i < vm->StackTop_ && i < Fa_VM::STACK_SIZE; ++i) {
        if (Fa_IS_OBJECT(vm->Stack_[i]))
            markObject(Fa_AS_OBJECT(vm->Stack_[i]));
    }

    // Call frames
    for (int i = 0; i < vm->FramesTop_ && i < Fa_VM::MAX_FRAMES; ++i)
        markObject(vm->Frames_[i].closure);

    // Open upvalues
    for (u32 i = 0, n = vm->OpenUpvalues_.size(); i < n; ++i)
        markObject(vm->OpenUpvalues_[i]);

    // Global slots are the live Fa_VM roots for globals.
    markValueArray(vm->GlobalSlots_);
}

void Fa_GarbageCollector::markObject(Fa_ObjHeader* p)
{
    if (!p || p->isMarked)
        return;

    p->isMarked = true;
    Grays_.push(p);
}

void Fa_GarbageCollector::blackenObject(Fa_ObjHeader* obj)
{
    switch (obj->type) {
    case Fa_ObjType::CLOSURE: {
        auto cl = static_cast<Fa_ObjClosure*>(obj);
        markObject(cl->function);
        for (u32 i = 0, n = cl->upValues.size(); i < n; ++i)
            markObject(cl->upValues[i]);
    } break;
    case Fa_ObjType::FUNCTION: {
        markObject(static_cast<Fa_ObjFunction*>(obj)->name);
        auto constants = &static_cast<Fa_ObjFunction*>(obj)->chunk->constants;
        markValueArray(*constants);
    } break;
    case Fa_ObjType::LIST: {
        auto lst = static_cast<Fa_ObjList*>(obj);
        markValueArray(lst->elements);
    } break;
    case Fa_ObjType::NATIVE: // natives are static
    case Fa_ObjType::STRING: // strings are managed with arena
        break;
    case Fa_ObjType::UPVALUE: {
        auto* uv = static_cast<ObjUpvalue*>(obj);
        if (uv->location == &uv->closed) {
            if (Fa_IS_OBJECT(uv->closed))
                markObject(Fa_AS_OBJECT(uv->closed));
        }
    } break;
    }
}

void Fa_GarbageCollector::sweep()
{
    u32 i = 0;
    while (i < All_.size()) {
        if (!All_[i]->isMarked) {
            delete All_[i];
            All_.erase(i);
            // delay decrement until erase succeeds
            CurrentSize_ -= sizeof(Fa_ObjHeader);
        } else {
            All_[i]->isMarked = false;
            ++i;
        }
    }
}

void Fa_GarbageCollector::markValueArray(Fa_Array<Fa_Value> const& arr)
{
    for (u32 i = 0, n = arr.size(); i < n; ++i) {
        if (Fa_IS_OBJECT(arr[i]))
            markObject(Fa_AS_OBJECT(arr[i]));
    }
}

void Fa_GarbageCollector::traceReferences()
{
    while (!Grays_.empty()) {
        Fa_ObjHeader* obj = Grays_.back();
        Grays_.pop();
        blackenObject(obj);
    }
}

// gc.cc
void Fa_GarbageCollector::sweepAll()
{
    for (u32 i = 0; i < All_.size(); ++i)
        delete All_[i];

    All_.clear();
    CurrentSize_ = 0;
}

} // namespace fairuz::runtime
