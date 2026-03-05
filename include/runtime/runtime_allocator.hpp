#ifndef _RUNTIME_ALLOCATOR_HPP
#define _RUNTIME_ALLOCATOR_HPP

#include "../allocator.hpp"

namespace mylang {
namespace runtime {

inline Allocator runtime_allocator("Runtime allocator");

}
}

#endif // _RUNTIME_ALLOCATOR_HPP
