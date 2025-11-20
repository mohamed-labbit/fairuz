#include "../../../include/runtime/vm/vm.hpp"

#include <unordered_set>


namespace mylang {
namespace runtime {

void GarbageCollector::registerObject(object::Value* obj)
{
    if (!obj)
    {
        std::cerr << "-- DEBUG: a null object was pushed to the garbage collector" << std::endl;
        return;
    }

    allObjects_.push_back(obj);
    youngGen_.push_back(obj);
    allocated_++;

    if (allocated_ >= threshold_) collect();
}

void GarbageCollector::addRoot(object::Value* root)
{
    if (!root)
    {
        std::cerr << "-- DEBUG: a null root was pushed to the garbage collector" << std::endl;
        return;
    }
    roots_.push_back(root);
}

void GarbageCollector::collect()
{
    // Mark phase
    std::unordered_set<object::Value*> marked;
    std::vector<object::Value*> worklist = roots_;

    while (!worklist.empty())
    {
        object::Value* obj = worklist.back();
        worklist.pop_back();
        if (marked.count(obj)) continue;
        marked.insert(obj);
        // Mark children (if list, dict, etc.)
        if (obj->isList())
            for (auto& item : obj->asList())
                worklist.push_back(const_cast<object::Value*>(&item));
    }

    // Sweep phase
    auto it = allObjects_.begin();
    while (it != allObjects_.end())
    {
        if (!marked.count(*it))
        {
            delete *it;
            it = allObjects_.erase(it);
            allocated_--;
        }
        else
        {
            ++it;
        }
    }

    youngGenCollections_++;
    // Promote survivors to old generation every 5 collections
    if (youngGenCollections_ % 5 == 0) promoteToOldGen();
}

}
}