#pragma once

#include "../../runtime/allocator/arena.hpp"


namespace mylang {
namespace parser {
namespace ast {

class ASTAllocator
{
 private:
  runtime::allocator::ArenaAllocator Allocator_;

 public:
  ASTAllocator() :
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename T, typename... Args>
  T* make(Args&&... args)
  {
    void* mem = Allocator_.allocate<std::byte>(sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }
};

inline ASTAllocator AST_allocator;

}
}
}