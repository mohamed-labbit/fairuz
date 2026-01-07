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
  std::vector<object::Value*> AllObjects_;
  std::vector<object::Value*> Roots_;
  std::size_t Threshold_ = 1000;
  std::size_t Allocated_ = 0;

  // Generational: young, old
  std::vector<object::Value*> YoungGen_;
  std::vector<object::Value*> OldGen_;
  std::int32_t YoungGenCollections_ = 0;

 public:
  void registerObject(object::Value* obj);
  void addRoot(object::Value* root);
  void collect();

 private:
  void promoteToOldGen()
  {
    for (auto* obj : YoungGen_) OldGen_.push_back(obj);
    YoungGen_.clear();
  }
};

}
}