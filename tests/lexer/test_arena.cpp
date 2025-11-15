#include "../../include/runtime/allocator/arena.hpp"
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <new>


using namespace mylang::runtime::allocator;

// ============================================================================
// Basic Allocation Tests
// ============================================================================

TEST(ArenaAllocatorTest, DefaultConstruction)
{
    ArenaAllocator arena;
    EXPECT_EQ(arena.total_allocations(), 0);
    EXPECT_EQ(arena.total_allocated(), 0);
    EXPECT_EQ(arena.active_blocks(), 0);
}

TEST(ArenaAllocatorTest, SingleIntAllocation)
{
    ArenaAllocator arena;
    int* ptr = arena.allocate<int>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 42;
    EXPECT_EQ(*ptr, 42);
    EXPECT_EQ(arena.total_allocations(), 1);
}

TEST(ArenaAllocatorTest, MultipleIntAllocations)
{
    ArenaAllocator arena;
    int* ptr1 = arena.allocate<int>();
    int* ptr2 = arena.allocate<int>();
    int* ptr3 = arena.allocate<int>();

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    *ptr1 = 1;
    *ptr2 = 2;
    *ptr3 = 3;

    EXPECT_EQ(*ptr1, 1);
    EXPECT_EQ(*ptr2, 2);
    EXPECT_EQ(*ptr3, 3);
    EXPECT_EQ(arena.total_allocations(), 3);
}

TEST(ArenaAllocatorTest, AllocateZeroCount)
{
    ArenaAllocator arena;
    int* ptr = arena.allocate<int>(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(ArenaAllocatorTest, AllocateArrayOfInts)
{
    ArenaAllocator arena;
    constexpr size_t count = 100;
    int* arr = arena.allocate<int>(count);

    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; ++i)
    {
        arr[i] = static_cast<int>(i);
    }

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(arr[i], static_cast<int>(i));
    }
}

TEST(ArenaAllocatorTest, AllocateLargeArray)
{
    ArenaAllocator arena;
    constexpr size_t count = 10000;
    double* arr = arena.allocate<double>(count);

    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; ++i)
    {
        arr[i] = static_cast<double>(i) * 1.5;
    }

    EXPECT_EQ(arr[0], 0.0);
    EXPECT_EQ(arr[count - 1], (count - 1) * 1.5);
}

// ============================================================================
// Type Tests
// ============================================================================

TEST(ArenaAllocatorTest, AllocateChar)
{
    ArenaAllocator arena;
    char* ptr = arena.allocate<char>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 'A';
    EXPECT_EQ(*ptr, 'A');
}

TEST(ArenaAllocatorTest, AllocateDouble)
{
    ArenaAllocator arena;
    double* ptr = arena.allocate<double>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 3.14159;
    EXPECT_DOUBLE_EQ(*ptr, 3.14159);
}

TEST(ArenaAllocatorTest, AllocateFloat)
{
    ArenaAllocator arena;
    float* ptr = arena.allocate<float>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 2.71828f;
    EXPECT_FLOAT_EQ(*ptr, 2.71828f);
}

TEST(ArenaAllocatorTest, AllocateLongLong)
{
    ArenaAllocator arena;
    std::int64_t* ptr = arena.allocate<std::int64_t>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 9223372036854775807LL;
    EXPECT_EQ(*ptr, 9223372036854775807LL);
}

// Custom POD struct
struct Point
{
    double x, y, z;
};

TEST(ArenaAllocatorTest, AllocatePODStruct)
{
    ArenaAllocator arena;
    Point* ptr = arena.allocate<Point>();
    ASSERT_NE(ptr, nullptr);
    ptr->x = 1.0;
    ptr->y = 2.0;
    ptr->z = 3.0;
    EXPECT_DOUBLE_EQ(ptr->x, 1.0);
    EXPECT_DOUBLE_EQ(ptr->y, 2.0);
    EXPECT_DOUBLE_EQ(ptr->z, 3.0);
}

struct LargeStruct
{
    char data[1024];
};

TEST(ArenaAllocatorTest, AllocateLargeStruct)
{
    ArenaAllocator arena;
    LargeStruct* ptr = arena.allocate<LargeStruct>();
    ASSERT_NE(ptr, nullptr);
    ptr->data[0] = 'X';
    ptr->data[1023] = 'Y';
    EXPECT_EQ(ptr->data[0], 'X');
    EXPECT_EQ(ptr->data[1023], 'Y');
}

// ============================================================================
// Fast Pool Tests (8, 16, 32, 64, 128, 256 byte allocations)
// ============================================================================

TEST(ArenaAllocatorTest, FastPool8Bytes)
{
    ArenaAllocator arena;
    struct Small8
    {
        char data[8];
    };
    Small8* ptr = arena.allocate<Small8>(100);
    ASSERT_NE(ptr, nullptr);
    ptr[0].data[0] = 'A';
    EXPECT_EQ(ptr[0].data[0], 'A');
}

TEST(ArenaAllocatorTest, FastPool16Bytes)
{
    ArenaAllocator arena;
    struct Small16
    {
        char data[16];
    };
    Small16* ptr = arena.allocate<Small16>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool32Bytes)
{
    ArenaAllocator arena;
    struct Small32
    {
        char data[32];
    };
    Small32* ptr = arena.allocate<Small32>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool64Bytes)
{
    ArenaAllocator arena;
    struct Small64
    {
        char data[64];
    };
    Small64* ptr = arena.allocate<Small64>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool128Bytes)
{
    ArenaAllocator arena;
    struct Small128
    {
        char data[128];
    };
    Small128* ptr = arena.allocate<Small128>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool256Bytes)
{
    ArenaAllocator arena;
    struct Small256
    {
        char data[256];
    };
    Small256* ptr = arena.allocate<Small256>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPoolMixedSizes)
{
    ArenaAllocator arena;

    struct S8
    {
        char data[8];
    };
    struct S16
    {
        char data[16];
    };
    struct S32
    {
        char data[32];
    };
    struct S64
    {
        char data[64];
    };

    S8* p8 = arena.allocate<S8>(10);
    S16* p16 = arena.allocate<S16>(10);
    S32* p32 = arena.allocate<S32>(10);
    S64* p64 = arena.allocate<S64>(10);

    ASSERT_NE(p8, nullptr);
    ASSERT_NE(p16, nullptr);
    ASSERT_NE(p32, nullptr);
    ASSERT_NE(p64, nullptr);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST(ArenaAllocatorTest, ResetClearsAllocations)
{
    ArenaAllocator arena;

    int* ptr1 = arena.allocate<int>(100);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_GT(arena.total_allocations(), 0);

    arena.reset();

    EXPECT_EQ(arena.total_allocations(), 0);
    EXPECT_EQ(arena.total_allocated(), 0);
    EXPECT_EQ(arena.active_blocks(), 0);
}

TEST(ArenaAllocatorTest, ReuseAfterReset)
{
    ArenaAllocator arena;
    int* ptr1 = arena.allocate<int>();
    *ptr1 = 42;
    EXPECT_EQ(*ptr1, 42);

    arena.reset();

    int* ptr2 = arena.allocate<int>();
    *ptr2 = 99;
    EXPECT_EQ(*ptr2, 99);
}

TEST(ArenaAllocatorTest, MultipleResets)
{
    ArenaAllocator arena;

    for (int i = 0; i < 10; ++i)
    {
        int* ptr = arena.allocate<int>(1000);
        ASSERT_NE(ptr, nullptr);
        arena.reset();
        EXPECT_EQ(arena.total_allocations(), 0);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST(ArenaAllocatorTest, StatisticsTracking)
{
    ArenaAllocator arena;

    EXPECT_EQ(arena.total_allocations(), 0);
    EXPECT_EQ(arena.total_allocated(), 0);

    int* ptr1 = arena.allocate<int>();
    EXPECT_EQ(arena.total_allocations(), 1);
    EXPECT_GE(arena.total_allocated(), sizeof(int));

    int* ptr2 = arena.allocate<int>(100);
    EXPECT_EQ(arena.total_allocations(), 2);
    EXPECT_GE(arena.total_allocated(), sizeof(int) * 101);
}

TEST(ArenaAllocatorTest, ActiveBlocksCount)
{
    ArenaAllocator arena;

    EXPECT_EQ(arena.active_blocks(), 0);

    // Small allocation triggers first block
    int* ptr1 = arena.allocate<int>();
    EXPECT_GT(arena.active_blocks(), 0);

    size_t initial_blocks = arena.active_blocks();

    // Many small allocations might stay in same block
    for (int i = 0; i < 10; ++i)
    {
        auto p = arena.allocate<int>();
        if (!p)
        {
            throw std::bad_alloc();
        }
    }

    // Large allocation forces new block
    auto p = arena.allocate<char>(1024 * 1024);
    if (!p)
        throw std::bad_alloc();
    EXPECT_GT(arena.active_blocks(), initial_blocks);
}

// ============================================================================
// Deallocation Tests (should be no-ops)
// ============================================================================

TEST(ArenaAllocatorTest, DeallocateIsNoOp)
{
    ArenaAllocator arena;

    int* ptr = arena.allocate<int>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 42;

    size_t allocations_before = arena.total_allocations();

    arena.deallocate(ptr);

    // Allocation should still be valid (arena doesn't free individual items)
    EXPECT_EQ(*ptr, 42);
}

TEST(ArenaAllocatorTest, DeallocateNullptr)
{
    ArenaAllocator arena;
    arena.deallocate<int>(nullptr);
    // Should not crash
}

// ============================================================================
// Alignment Tests
// ============================================================================

TEST(ArenaAllocatorTest, ProperAlignment)
{
    ArenaAllocator arena;

    double* d = arena.allocate<double>();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(d) % alignof(double), 0);

    std::int64_t* ll = arena.allocate<std::int64_t>();
    ASSERT_NE(ll, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ll) % alignof(std::int64_t), 0);
}

struct alignas(64) AlignedStruct
{
    char data[32];
};

TEST(ArenaAllocatorTest, CustomAlignment)
{
    ArenaAllocator arena;
    AlignedStruct* ptr = arena.allocate<AlignedStruct>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0);
}

// ============================================================================
// Growth Strategy Tests
// ============================================================================

TEST(ArenaAllocatorTest, ExponentialGrowth)
{
    ArenaAllocator arena(static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL));

    // Force multiple block allocations
    for (int i = 0; i < 10; ++i)
    {
        char* ptr = arena.allocate<char>(1024);
        ASSERT_NE(ptr, nullptr);
    }

    EXPECT_GT(arena.active_blocks(), 1);
}

TEST(ArenaAllocatorTest, LinearGrowth)
{
    ArenaAllocator arena(static_cast<int>(ArenaAllocator::GrowthStrategy::LINEAR));

    // Force multiple block allocations
    for (int i = 0; i < 10; ++i)
    {
        char* ptr = arena.allocate<char>(1024);
        ASSERT_NE(ptr, nullptr);
    }

    EXPECT_GT(arena.active_blocks(), 1);
}

// ============================================================================
// Large Allocation Tests
// ============================================================================

TEST(ArenaAllocatorTest, AllocateMaxBlockSize)
{

    // Allocate close to MAX_BLOCK_SIZE
    constexpr size_t large_size = MAX_BLOCK_SIZE / sizeof(char32_t);
    ArenaAllocator arena(large_size);
    char32_t* ptr = arena.allocate<char32_t>(large_size);
    ASSERT_NE(ptr, nullptr);

    ptr[0] = 'A';
    ptr[large_size - 1] = 'Z';

    EXPECT_EQ(ptr[0], 'A');
    EXPECT_EQ(ptr[large_size - 1], 'Z');
}

TEST(ArenaAllocatorTest, AllocateExceedsMaxBlockSize)
{
    ArenaAllocator arena;
    // Try to allocate more than MAX_BLOCK_SIZE
    constexpr size_t too_large = MAX_BLOCK_SIZE + 1024;
    // Should return nullptr as it exceeds limit
    char* ptr = nullptr;  // to silence no-discard warning
    EXPECT_THROW((ptr = arena.allocate<char>(too_large)), std::bad_alloc);
}

TEST(ArenaAllocatorTest, MultipleBlocksRequired)
{
    ArenaAllocator arena;

    std::vector<int*> ptrs;

    // Allocate enough to require multiple blocks
    for (int i = 0; i < 10; ++i)
    {
        int* ptr = arena.allocate<int>(10000);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        *ptr = i;
    }

    // Verify all allocations
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(*ptrs[i], i);
    }

    EXPECT_GT(arena.active_blocks(), 1);
}

// ============================================================================
// Debug Mode Tests
// ============================================================================

TEST(ArenaAllocatorTest, DebugModeEnabled)
{
    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), nullptr,
      true  // Enable debug
    );

    int* ptr = arena.allocate<int>();
    ASSERT_NE(ptr, nullptr);

    // In debug mode, allocation should be tracked
    EXPECT_TRUE(arena.verify_allocation(ptr));
}

TEST(ArenaAllocatorTest, DebugModeDisabled)
{
    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), nullptr,
      false  // Disable debug
    );

    int* ptr = arena.allocate<int>();
    ASSERT_NE(ptr, nullptr);

    // Without debug mode, verify should return true (not tracked)
    EXPECT_TRUE(arena.verify_allocation(ptr));
}

TEST(ArenaAllocatorTest, VerifyInvalidPointer)
{
    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), nullptr,
      true  // Enable debug
    );

    int stack_var = 42;

    // Stack pointer should not be verified as arena allocation
    EXPECT_FALSE(arena.verify_allocation(&stack_var));
}

// ============================================================================
// OOM Handler Tests
// ============================================================================

TEST(ArenaAllocatorTest, OOMHandlerNotCalled)
{
    bool handler_called = false;
    auto oom_handler = [&](size_t requested) {
        handler_called = true;
        return false;
    };

    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), oom_handler);

    int* ptr = arena.allocate<int>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_FALSE(handler_called);
}

TEST(ArenaAllocatorTest, OOMHandlerCalledOnFailure)
{
    bool handler_called = false;
    size_t requested_size = 0;

    auto oom_handler = [&](size_t requested) {
        handler_called = true;
        requested_size = requested;
        return false;  // Don't retry
    };

    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), oom_handler);

    // Try to allocate more than allowed
    constexpr size_t too_large = MAX_BLOCK_SIZE + 1024;
    char* ptr = nullptr;

    EXPECT_THROW((ptr = arena.allocate<char>(too_large)), std::bad_alloc);
    EXPECT_TRUE(handler_called);
    EXPECT_GT(requested_size, 0);
}

TEST(ArenaAllocatorTest, OOMHandlerRetry)
{
    int call_count = 0;

    auto oom_handler = [&](size_t requested) {
        call_count++;
        return call_count < 2;  // Retry once
    };

    ArenaAllocator arena(
      static_cast<int>(ArenaAllocator::GrowthStrategy::EXPONENTIAL), alignof(std::max_align_t), oom_handler);

    // Allocate oversized
    constexpr size_t too_large = MAX_BLOCK_SIZE + 1024;
    char* ptr = nullptr;

    EXPECT_THROW((ptr = arena.allocate<char>(too_large)), std::bad_alloc);
    EXPECT_EQ(call_count, 2);  // Called twice (initial + 1 retry)
}

TEST(ArenaAllocatorTest, ManySmallAllocations)
{
    ArenaAllocator arena;

    constexpr int count = 10;
    std::vector<int*> ptrs;

    for (int i = 0; i < count; ++i)
    {
        int* ptr = arena.allocate<int>();
        ASSERT_NE(ptr, nullptr);
        *ptr = i;
        ptrs.push_back(ptr);
    }

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(*ptrs[i], i);
    }

    EXPECT_EQ(arena.total_allocations(), count);
}

TEST(ArenaAllocatorTest, MixedSizeAllocations)
{
    ArenaAllocator arena;

    std::vector<void*> ptrs;

    for (int i = 0; i < 10; ++i)
    {
        size_t size = (i % 10) + 1;
        if (i % 3 == 0)
        {
            int* ptr = arena.allocate<int>(size);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        }
        else if (i % 3 == 1)
        {
            double* ptr = arena.allocate<double>(size);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        }
        else
        {
            char* ptr = arena.allocate<char>(size * 100);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        }
    }

    EXPECT_EQ(arena.total_allocations(), 10);
}

TEST(ArenaAllocatorTest, AlternatingLargeSmall)
{
    ArenaAllocator arena;
    std::size_t large_amount = 10000, small_amount = 1;

    for (int i = 0; i < 10; ++i)
    {
        if (i % 2 == 0)
        {
            char* large = arena.allocate<char>(large_amount);
            ASSERT_NE(large, nullptr);
        }
        else
        {
            int* small = arena.allocate<int>(small_amount);
            ASSERT_NE(small, nullptr);
        }
    }

    EXPECT_EQ(arena.total_allocations(), 10);
}

// ============================================================================
// Block Tests
// ============================================================================

TEST(ArenaAllocatorTest, AllocateBlockDirect)
{
    ArenaAllocator arena;

    unsigned char* ptr = arena.allocate_block(1024);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(arena.active_blocks(), 1);

    ptr[0] = 'A';
    ptr[1023] = 'Z';
    EXPECT_EQ(ptr[0], 'A');
    EXPECT_EQ(ptr[1023], 'Z');
}

TEST(ArenaAllocatorTest, AllocateFastBlock)
{
    ArenaAllocator arena;

    unsigned char* ptr = arena.allocate_fast_block<64>(1024);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(arena.active_blocks(), 1);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(ArenaAllocatorTest, AllocateOne)
{
    ArenaAllocator arena;
    char* ptr = arena.allocate<char>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 'X';
    EXPECT_EQ(*ptr, 'X');
}

TEST(ArenaAllocatorTest, ConsecutiveAllocations)
{
    ArenaAllocator arena;

    int* ptr1 = arena.allocate<int>();
    int* ptr2 = arena.allocate<int>();
    int* ptr3 = arena.allocate<int>();

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Should be consecutive in memory (arena bump allocator)
    // Note: This might not always be true due to alignment
    size_t diff1 = reinterpret_cast<uintptr_t>(ptr2) - reinterpret_cast<uintptr_t>(ptr1);
    size_t diff2 = reinterpret_cast<uintptr_t>(ptr3) - reinterpret_cast<uintptr_t>(ptr2);

    EXPECT_GE(diff1, sizeof(int));
    EXPECT_GE(diff2, sizeof(int));
}

TEST(ArenaAllocatorTest, ResetAndAllocateAgain)
{
    ArenaAllocator arena;

    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 10; ++i)
        {
            int* ptr = arena.allocate<int>();
            ASSERT_NE(ptr, nullptr);
            *ptr = i;
        }

        EXPECT_EQ(arena.total_allocations(), 10);
        arena.reset();
        EXPECT_EQ(arena.total_allocations(), 0);
    }
}

// ============================================================================
// ArenaBlock Tests
// ============================================================================

TEST(ArenaBlockTest, DefaultConstruction)
{
    ArenaBlock block;
    EXPECT_EQ(block.size(), DEFAULT_BLOCK_SIZE);
    EXPECT_NE(block.begin(), nullptr);
}

TEST(ArenaBlockTest, CustomSize)
{
    ArenaBlock block(1024);
    EXPECT_EQ(block.size(), 1024);
}

TEST(ArenaBlockTest, AllocateFromBlock)
{
    ArenaBlock block(1024);
    unsigned char* ptr = block.reserve(128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_LE(block.remaining(), 1024);
}

TEST(ArenaBlockTest, BlockExhaustion)
{
    ArenaBlock block(128);
    unsigned char* ptr1 = block.reserve(64);
    ASSERT_NE(ptr1, nullptr);

    unsigned char* ptr2 = block.reserve(64);
    ASSERT_NE(ptr2, nullptr);

    // Should fail - not enough space
    unsigned char* ptr3 = block.reserve(64);
    EXPECT_EQ(ptr3, nullptr);
}

TEST(ArenaBlockTest, InvalidAlignment)
{
    ArenaBlock block(1024);
    EXPECT_THROW(block.allocate(64, /*alignment=*/-1), std::invalid_argument);
    EXPECT_THROW(block.allocate(64, /*alignment=*/-1), std::invalid_argument);
}

TEST(ArenaBlockTest, MoveConstruction)
{
    ArenaBlock block1(1024);
    unsigned char* ptr = block1.reserve(128);
    ASSERT_NE(ptr, nullptr);

    ArenaBlock block2(std::move(block1));
    EXPECT_EQ(block2.size(), 1024);
    EXPECT_EQ(block1.size(), 0);
    EXPECT_EQ(block1.begin(), nullptr);
}

// ============================================================================
// Performance Benchmarks (as tests)
// ============================================================================

/*
 * 
TEST(ArenaAllocatorPerformance, AllocateMillionInts)
{
    ArenaAllocator arena;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000000; ++i)
    {
        int* ptr = arena.allocate<int>();
        ASSERT_NE(ptr, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Allocated 1 million ints in " << duration.count() << "ms\n";
    EXPECT_EQ(arena.total_allocations(), 1000000);
}
 */

TEST(ArenaAllocatorPerformance, ResetPerformance)
{
    ArenaAllocator arena;

    // Allocate lots of memory
    for (int i = 0; i < 10; ++i)
    {
        auto p = arena.allocate<char>(1024);
        if (!p)
            throw std::bad_alloc();
    }

    auto start = std::chrono::high_resolution_clock::now();
    arena.reset();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Reset took " << duration.count() << "μs\n";

    EXPECT_EQ(arena.total_allocations(), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}