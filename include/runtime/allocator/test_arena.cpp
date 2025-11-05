#include "arena.h"

#include <cassert>
#include <cstdint>
#include <iostream>


namespace mylang {
namespace runtime {
namespace allocator {
namespace tests {

static void test_basic_allocate()
{
    ArenaAllocator alloc;
    // allocate 10 ints
    int* p = alloc.allocate<int>(10);
    assert(p != nullptr);
    assert(alloc.total_allocations() == 1);
    assert(alloc.total_allocated() == 10 * sizeof(int));

    // deallocate is a no-op but must not crash
    alloc.deallocate(p, 10);
}

static void test_fast_pool_and_alignment()
{
    ArenaAllocator alloc;

    // Type of size 8 and alignment 8 should use the fast pool for sizeof==8
    struct alignas(8) S8
    {
        char data[8];
    };
    static_assert(sizeof(S8) == 8 && alignof(S8) == 8);

    // allocate two objects (alloc_size == 16) to exercise fast-pool block allocation + intra-block allocation
    S8* s = alloc.allocate<S8>(2);
    assert(s != nullptr);
    // returned pointer must satisfy the alignment
    assert((reinterpret_cast<std::uintptr_t>(s) % alignof(S8)) == 0);

    // allocate a single object of a larger alignment (should fall back to block allocation path)
    struct alignas(64) A64
    {
        char data[16];
    };
    static_assert(alignof(A64) == 64);
    A64* a = alloc.allocate<A64>(1);
    assert(a != nullptr);
    assert((reinterpret_cast<std::uintptr_t>(a) % alignof(A64)) == 0);
}

static void test_debug_tracking_and_reset()
{
    // enable debug features so allocations are tracked
    ArenaAllocator alloc(static_cast<int>(0), alignof(std::max_align_t), nullptr, true);

    // allocate and verify tracking
    using T = int;
    T* p = alloc.allocate<T>(3);
    assert(p != nullptr);
    assert(alloc.verify_allocation(p) == true);

    // deallocate should remove tracking entry
    alloc.deallocate(p, 3);
    assert(alloc.verify_allocation(p) == false);

    // reset should clear stats and active blocks
    alloc.reset();
    assert(alloc.total_allocated() == 0);
    assert(alloc.total_allocations() == 0);
    assert(alloc.active_blocks() == 0);
}

static void test_allocate_exceeds_max_block()
{
    ArenaAllocator alloc;
    // Request more than MAX_BLOCK_SIZE bytes via allocate<T>
    // Use char so sizeof == 1 and count == MAX_BLOCK_SIZE + 1 triggers the check
    void* r = alloc.allocate<char>(MAX_BLOCK_SIZE + 1);
    assert(r == nullptr);
}

inline void run_arena_tests()
{
    try
    {
        test_basic_allocate();
        test_fast_pool_and_alignment();
        test_debug_tracking_and_reset();
        test_allocate_exceeds_max_block();
    } catch (...)
    {
        std::cerr << "Arena tests failed (exception caught)\n";
        throw;
    }
    std::cerr << "Arena tests passed\n";
}

}  // namespace tests
}  // namespace allocator
}  // namespace runtime
}  // namespace mylang


int main(void)
{
    mylang::runtime::allocator::tests::run_arena_tests();
    return 0;
}