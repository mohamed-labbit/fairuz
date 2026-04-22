#include "../fairuz/../fairuz/arena.hpp"

#include <gtest/gtest.h>

using namespace fairuz;

class TestAllocator {
private:
    Fa_ArenaAllocator allocator_;

public:
    TestAllocator()
        : allocator_(Fa_ArenaAllocator::GrowthStrategy::LINEAR)
    {
    }

    Fa_ArenaAllocator& get() noexcept { return std::ref<Fa_ArenaAllocator>(allocator_); }

    template<typename T, typename... Args>
    T* allocate(size_t count, Args&&... m_args)
    {
        void* mem = allocator_.allocate(count * sizeof(T));
        if (!mem)
            return nullptr;
        return new (mem) T(std::forward<Args>(m_args)...);
    }
};

inline TestAllocator test_allocator;

TEST(ArenaAllocatorTest, SingleIntAllocation)
{
    TestAllocator arena;
    int* ptr = arena.allocate<int>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 42;
    EXPECT_EQ(*ptr, 42);
}

TEST(ArenaAllocatorTest, MultipleIntAllocations)
{
    TestAllocator arena;
    int* ptr1 = arena.allocate<int>(1);
    int* ptr2 = arena.allocate<int>(1);
    int* ptr3 = arena.allocate<int>(1);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    *ptr1 = 1;
    *ptr2 = 2;
    *ptr3 = 3;

    EXPECT_EQ(*ptr1, 1);
    EXPECT_EQ(*ptr2, 2);
    EXPECT_EQ(*ptr3, 3);
}

TEST(ArenaAllocatorTest, AllocateZeroCount)
{
    TestAllocator arena;
    int* ptr = arena.allocate<int>(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(ArenaAllocatorTest, AllocateArrayOfInts)
{
    TestAllocator arena;
    constexpr size_t count = 100;
    int* arr = arena.allocate<int>(count);

    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; i += 1)
        arr[i] = static_cast<int>(i);

    for (size_t i = 0; i < count; i += 1)
        EXPECT_EQ(arr[i], static_cast<int>(i));
}

TEST(ArenaAllocatorTest, AllocateLargeArray)
{
    TestAllocator arena;
    constexpr size_t count = 10000;
    f64* arr = arena.allocate<f64>(count);

    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; i += 1)
        arr[i] = static_cast<f64>(i) * 1.5;

    EXPECT_EQ(arr[0], 0.0);
    EXPECT_EQ(arr[count - 1], (count - 1) * 1.5);
}

TEST(ArenaAllocatorTest, AllocateChar)
{
    TestAllocator arena;
    char* ptr = arena.allocate<char>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 'A';
    EXPECT_EQ(*ptr, 'A');
}

TEST(ArenaAllocatorTest, AllocateDouble)
{
    TestAllocator arena;
    f64* ptr = arena.allocate<f64>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 3.14159;
    EXPECT_DOUBLE_EQ(*ptr, 3.14159);
}

TEST(ArenaAllocatorTest, AllocateFloat)
{
    TestAllocator arena;
    float* ptr = arena.allocate<float>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 2.71828f;
    EXPECT_FLOAT_EQ(*ptr, 2.71828f);
}

TEST(ArenaAllocatorTest, AllocateLongLong)
{
    TestAllocator arena;
    i64* ptr = arena.allocate<i64>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 9223372036854775807LL;
    EXPECT_EQ(*ptr, 9223372036854775807LL);
}

struct Point {
    f64 x, y, z;
};

TEST(ArenaAllocatorTest, AllocatePODStruct)
{
    TestAllocator arena;
    Point* ptr = arena.allocate<Point>(1);

    ASSERT_NE(ptr, nullptr);

    ptr->x = 1.0;
    ptr->y = 2.0;
    ptr->z = 3.0;

    EXPECT_DOUBLE_EQ(ptr->x, 1.0);
    EXPECT_DOUBLE_EQ(ptr->y, 2.0);
    EXPECT_DOUBLE_EQ(ptr->z, 3.0);
}

struct LargeStruct {
    char data[1024];
};

TEST(ArenaAllocatorTest, AllocateLargeStruct)
{
    TestAllocator arena;
    LargeStruct* ptr = arena.allocate<LargeStruct>(1);
    ASSERT_NE(ptr, nullptr);
    ptr->data[0] = 'X';
    ptr->data[1023] = 'Y';
    EXPECT_EQ(ptr->data[0], 'X');
    EXPECT_EQ(ptr->data[1023], 'Y');
}

TEST(ArenaAllocatorTest, FastPool8Bytes)
{
    TestAllocator arena;
    struct Small8 {
        char data[8];
    };
    Small8* ptr = arena.allocate<Small8>(100);
    ASSERT_NE(ptr, nullptr);
    ptr[0].data[0] = 'A';
    EXPECT_EQ(ptr[0].data[0], 'A');
}

TEST(ArenaAllocatorTest, FastPool16Bytes)
{
    TestAllocator arena;
    struct Small16 {
        char data[16];
    };
    Small16* ptr = arena.allocate<Small16>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool32Bytes)
{
    TestAllocator arena;
    struct Small32 {
        char data[32];
    };
    Small32* ptr = arena.allocate<Small32>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool64Bytes)
{
    TestAllocator arena;
    struct Small64 {
        char data[64];
    };
    Small64* ptr = arena.allocate<Small64>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool128Bytes)
{
    TestAllocator arena;
    struct Small128 {
        char data[128];
    };
    Small128* ptr = arena.allocate<Small128>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPool256Bytes)
{
    TestAllocator arena;
    struct Small256 {
        char data[256];
    };
    Small256* ptr = arena.allocate<Small256>(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, FastPoolMixedSizes)
{
    TestAllocator arena;

    struct S8 {
        char data[8];
    };
    struct S16 {
        char data[16];
    };
    struct S32 {
        char data[32];
    };
    struct S64 {
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

TEST(ArenaAllocatorTest, ReuseAfterReset)
{
    TestAllocator arena;
    int* ptr1 = arena.allocate<int>(1);
    *ptr1 = 42;
    EXPECT_EQ(*ptr1, 42);

    arena.get().reset();

    int* ptr2 = arena.allocate<int>(1);
    *ptr2 = 99;
    EXPECT_EQ(*ptr2, 99);
}

TEST(ArenaAllocatorTest, MultipleResets)
{
    TestAllocator arena;

    for (int i = 0; i < 10; i += 1) {
        int* ptr = arena.allocate<int>(1000);
        ASSERT_NE(ptr, nullptr);
        arena.get().reset();
    }
}

TEST(ArenaAllocatorTest, ProperAlignment)
{
    TestAllocator arena;

    f64* d = arena.allocate<f64>(1);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(d) % alignof(f64), 0);

    i64* ll = arena.allocate<i64>(1);
    ASSERT_NE(ll, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ll) % alignof(i64), 0);
}

struct alignas(64) AlignedStruct {
    char data[32];
};

TEST(ArenaAllocatorTest, CustomAlignment)
{
    TestAllocator arena;
    AlignedStruct* ptr = arena.allocate<AlignedStruct>(1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0);
}

TEST(ArenaAllocatorTest, MultipleBlocksRequired)
{
    TestAllocator arena;

    std::vector<int*> ptrs;

    for (int i = 0; i < 10; i += 1) {
        int* ptr = arena.allocate<int>(10000);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        *ptr = i;
    }

    for (int i = 0; i < 10; i += 1)
        EXPECT_EQ(*ptrs[i], i);
}

TEST(ArenaAllocatorTest, ManySmallAllocations)
{
    TestAllocator arena;

    constexpr int count = 10;
    std::vector<int*> ptrs;

    for (int i = 0; i < count; i += 1) {
        int* ptr = arena.allocate<int>(1);
        ASSERT_NE(ptr, nullptr);
        *ptr = i;
        ptrs.push_back(ptr);
    }

    for (int i = 0; i < count; i += 1)
        EXPECT_EQ(*ptrs[i], i);
}

TEST(ArenaAllocatorTest, MixedSizeAllocations)
{
    TestAllocator arena;

    std::vector<void*> ptrs;

    for (int i = 0; i < 10; i += 1) {
        size_t m_size = (i % 10) + 1;
        if (i % 3 == 0) {
            int* ptr = arena.allocate<int>(m_size);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        } else if (i % 3 == 1) {
            f64* ptr = arena.allocate<f64>(m_size);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        } else {
            char* ptr = arena.allocate<char>(m_size * 100);
            ASSERT_NE(ptr, nullptr);
            ptrs.push_back(ptr);
        }
    }
}

TEST(ArenaAllocatorTest, AlternatingLargeSmall)
{
    TestAllocator arena;
    size_t large_amount = 10000, small_amount = 1;

    for (int i = 0; i < 10; i += 1) {
        if (i % 2 == 0) {
            char* large = arena.allocate<char>(large_amount);
            ASSERT_NE(large, nullptr);
        } else {
            int* small = arena.allocate<int>(small_amount);
            ASSERT_NE(small, nullptr);
        }
    }
}

TEST(ArenaAllocatorTest, AllocateBlockDirect)
{
    TestAllocator arena;

    unsigned char* ptr = arena.get().allocate_block(1024);
    ASSERT_NE(ptr, nullptr);

    ptr[0] = 'A';
    ptr[1023] = 'Z';
    EXPECT_EQ(ptr[0], 'A');
    EXPECT_EQ(ptr[1023], 'Z');
}

TEST(ArenaAllocatorTest, AllocateOne)
{
    TestAllocator arena;
    char* ptr = arena.allocate<char>(1);
    ASSERT_NE(ptr, nullptr);
    *ptr = 'X';
    EXPECT_EQ(*ptr, 'X');
}

TEST(ArenaAllocatorTest, ConsecutiveAllocations)
{
    TestAllocator arena;

    int* ptr1 = arena.allocate<int>(1);
    int* ptr2 = arena.allocate<int>(1);
    int* ptr3 = arena.allocate<int>(1);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    size_t diff1 = reinterpret_cast<uintptr_t>(ptr2) - reinterpret_cast<uintptr_t>(ptr1);
    size_t diff2 = reinterpret_cast<uintptr_t>(ptr3) - reinterpret_cast<uintptr_t>(ptr2);

    EXPECT_GE(diff1, sizeof(int));
    EXPECT_GE(diff2, sizeof(int));
}

TEST(F_ArenaBlockTest, DefaultConstruction)
{
    Fa_ArenaBlock block;
    EXPECT_EQ(block.size(), DEFAULT_BLOCK_SIZE);
    EXPECT_NE(block.begin(), nullptr);
}

TEST(F_ArenaBlockTest, CustomSize)
{
    Fa_ArenaBlock block(1024);
    EXPECT_EQ(block.size(), 1024);
}

TEST(F_ArenaBlockTest, AllocateFromBlock)
{
    Fa_ArenaBlock block(1024);
    unsigned char* ptr = block.reserve(128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_LE(block.remaining(), 1024);
}

TEST(F_ArenaBlockTest, BlockExhaustion)
{
    Fa_ArenaBlock block(128);
    unsigned char* ptr1 = block.reserve(64);
    ASSERT_NE(ptr1, nullptr);

    unsigned char* ptr2 = block.reserve(64);
    ASSERT_NE(ptr2, nullptr);

    unsigned char* ptr3 = block.reserve(64);
    EXPECT_EQ(ptr3, nullptr);
}

TEST(F_ArenaBlockTest, MoveConstruction)
{
    Fa_ArenaBlock block1(1024);
    unsigned char* ptr = block1.reserve(128);
    ASSERT_NE(ptr, nullptr);

    Fa_ArenaBlock block2(std::move(block1));
    EXPECT_EQ(block2.size(), 1024);
    EXPECT_EQ(block1.size(), 0);
    EXPECT_EQ(block1.begin(), nullptr);
}

#ifdef FAIRUZ_DEBUG
TEST(ArenaAllocatorStatsTest, TracksRequestedBytesPeakAndBlocks)
{
    Fa_ArenaAllocator allocator(Fa_ArenaAllocator::GrowthStrategy::LINEAR, nullptr, true);

    void* first = allocator.allocate(13);
    void* second = allocator.allocate(29);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    DetailedAllocStats const& after_alloc = allocator.stats();
    EXPECT_EQ(after_alloc.TotalAllocations, 2u);
    EXPECT_EQ(after_alloc.TotalDeallocations, 0u);
    EXPECT_EQ(after_alloc.TotalAllocated, 42u);
    EXPECT_EQ(after_alloc.TotalDeallocated, 0u);
    EXPECT_EQ(after_alloc.CurrentlyAllocated, 42u);
    EXPECT_EQ(after_alloc.PeakAllocated, 42u);
    EXPECT_EQ(after_alloc.ActiveBlocks, 1u);
    EXPECT_EQ(after_alloc.BlockAllocations, 1u);
    EXPECT_EQ(after_alloc.BlockExpansions, 0u);
    EXPECT_TRUE(allocator.verify_allocation(first));
    EXPECT_TRUE(allocator.verify_allocation(second));

    allocator.deallocate(second, 29);

    DetailedAllocStats const& after_dealloc = allocator.stats();
    EXPECT_EQ(after_dealloc.TotalDeallocations, 1u);
    EXPECT_EQ(after_dealloc.TotalDeallocated, 29u);
    EXPECT_EQ(after_dealloc.CurrentlyAllocated, 13u);
    EXPECT_EQ(after_dealloc.PeakAllocated, 42u);
    EXPECT_EQ(after_dealloc.ActiveBlocks, 1u);
    EXPECT_TRUE(allocator.verify_allocation(first));
    EXPECT_FALSE(allocator.verify_allocation(second));
}

TEST(ArenaAllocatorStatsTest, DeallocateReclaimsAlignmentPadding)
{
    Fa_ArenaAllocator allocator(Fa_ArenaAllocator::GrowthStrategy::LINEAR, nullptr, true);

    void* first = allocator.allocate(1);
    void* aligned = allocator.allocate(1, 64);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(aligned, nullptr);

    allocator.deallocate(aligned, 1);

    void* third = allocator.allocate(1);
    ASSERT_NE(third, nullptr);
    EXPECT_NE(third, aligned);

    DetailedAllocStats const& stats = allocator.stats();
    EXPECT_EQ(stats.TotalAllocations, 3u);
    EXPECT_EQ(stats.TotalDeallocations, 1u);
    EXPECT_EQ(stats.CurrentlyAllocated, 2u);
}

TEST(ArenaAllocatorStatsTest, DistinguishesInvalidAndDoubleFreeAttempts)
{
    Fa_ArenaAllocator allocator(Fa_ArenaAllocator::GrowthStrategy::LINEAR, nullptr, true);

    void* first = allocator.allocate(8);
    void* second = allocator.allocate(16);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    allocator.deallocate(first, 8);

    DetailedAllocStats const& after_invalid = allocator.stats();
    EXPECT_EQ(after_invalid.InvalidFreeAttempts, 1u);
    EXPECT_EQ(after_invalid.DoubleFreeAttempts, 0u);
    EXPECT_EQ(after_invalid.TotalDeallocations, 0u);

    allocator.deallocate(second, 16);
    allocator.deallocate(second, 16);

    DetailedAllocStats const& after_double = allocator.stats();
    EXPECT_EQ(after_double.InvalidFreeAttempts, 1u);
    EXPECT_EQ(after_double.DoubleFreeAttempts, 1u);
    EXPECT_EQ(after_double.TotalDeallocations, 1u);
    EXPECT_EQ(after_double.TotalDeallocated, 16u);
    EXPECT_EQ(after_double.CurrentlyAllocated, 8u);
}

TEST(ArenaAllocatorStatsTest, TracksBlockAllocationAndExpansionCounts)
{
    Fa_ArenaAllocator allocator(Fa_ArenaAllocator::GrowthStrategy::LINEAR, nullptr, true);

    ASSERT_NE(allocator.allocate(DEFAULT_BLOCK_SIZE), nullptr);
    ASSERT_NE(allocator.allocate(DEFAULT_BLOCK_SIZE), nullptr);

    DetailedAllocStats const& stats = allocator.stats();
    EXPECT_EQ(stats.ActiveBlocks, 2u);
    EXPECT_EQ(stats.BlockAllocations, 2u);
    EXPECT_EQ(stats.BlockExpansions, 1u);
}
#endif // FAIRUZ_DEBUG

TEST(ArenaAllocatorPerformance, ResetPerformance)
{
    TestAllocator arena;

    for (int i = 0; i < 10; i += 1) {
        char* p = arena.allocate<char>(1024);
        if (p == nullptr)
            throw std::bad_alloc();
    }

    auto start = std::chrono::high_resolution_clock::now();
    arena.get().reset();
    auto m_end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_end - start);
    std::cout << "Reset took " << duration.count() << "μs\n";
}
