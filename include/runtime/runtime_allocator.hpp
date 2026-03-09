#ifndef RUNTIME_ALLOCATOR_HPP
#define RUNTIME_ALLOCATOR_HPP

#include "../allocator.hpp"

namespace mylang {
namespace runtime {

inline Allocator runtime_allocator("Runtime allocator");

}
}

#endif // RUNTIME_ALLOCATOR_HPP
