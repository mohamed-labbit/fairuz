#pragma once

#include "../object/object.hpp"
#include <vector>


namespace mylang {
namespace runtime {

// ============================================================================
// GARBAGE COLLECTOR - Mark & Sweep with generational collection
// ============================================================================
class GarbageCollector
{
   private:
    std::vector<object::Value*> allObjects_;
    std::vector<object::Value*> roots_;
    std::size_t threshold_ = 1000;
    std::size_t allocated_ = 0;

    // Generational: young, old
    std::vector<object::Value*> youngGen_;
    std::vector<object::Value*> oldGen_;
    int youngGenCollections_ = 0;

   public:
    void registerObject(object::Value* obj);

    void addRoot(object::Value* root);

    void collect();

   private:
    void promoteToOldGen()
    {
        for (auto* obj : youngGen_)
        {
            oldGen_.push_back(obj);
        }
        youngGen_.clear();
    }
};

}
}