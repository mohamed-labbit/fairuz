#pragma once

#include "../runtime/allocator/arena.hpp"

namespace mylang {
namespace ast {

class ASTAllocator {
private:
    runtime::allocator::ArenaAllocator Allocator_;

public:
    ASTAllocator()
        : Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
    {
        Allocator_.setName("AST Allocator");
    }

    template<typename T, typename... Args>
    T* make(Args&&... args)
    {
        void* mem = Allocator_.allocate(sizeof(T));
        if (!mem)
            return nullptr;

        return new (mem) T(std::forward<Args>(args)...);
    }

    std::string toString(bool const verbose) const
    {
        return Allocator_.toString(verbose);
    }
};

inline ASTAllocator AST_allocator;

} // ast
} // mylang
