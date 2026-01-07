#include "../../../include/runtime/vm/gc.hpp"
#include "../../../include/diag/diagnostic.hpp"

#include <unordered_set>


namespace mylang {
namespace runtime {

void GarbageCollector::registerObject(object::Value* obj)
{
  if (!obj)
  {
#if DEBUG_PRINT
    // std::cerr << "-- DEBUG: a null object was pushed to the garbage collector" << std::endl;
    diagnostic::engine.emit("-- DEBUG : null object push to gc");
#endif
    return;
  }

  AllObjects_.push_back(obj);
  YoungGen_.push_back(obj);
  Allocated_++;

  if (Allocated_ >= Threshold_) collect();
}

void GarbageCollector::addRoot(object::Value* root)
{
  if (!root)
  {
#if DEBUG_PRINT
    // std::cerr << "-- DEBUG: a null root was pushed to the garbage collector" << std::endl;
    diagnostic::engine.emit("-- DEBUG : null root push to gc");
#endif
    return;
  }
  Roots_.push_back(root);
}

void GarbageCollector::collect()
{
  // Mark phase
  std::unordered_set<object::Value*> marked;
  std::vector<object::Value*> worklist = Roots_;

  while (!worklist.empty())
  {
    object::Value* obj = worklist.back();
    worklist.pop_back();
    if (marked.count(obj)) continue;
    marked.insert(obj);
    // Mark children (if list, dict, etc.)
    if (obj->isList())
      for (auto& item : obj->asList()) worklist.push_back(const_cast<object::Value*>(&item));
  }

  // Sweep phase
  auto it = AllObjects_.begin();
  while (it != AllObjects_.end())
  {
    if (!marked.count(*it))
    {
      delete *it;
      it = AllObjects_.erase(it);
      Allocated_--;
    }
    else
    {
      ++it;
    }
  }

  YoungGenCollections_++;
  // Promote survivors to old generation every 5 collections
  if (YoungGenCollections_ % 5 == 0) promoteToOldGen();
}

}
}