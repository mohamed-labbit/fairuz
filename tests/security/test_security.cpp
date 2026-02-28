// Author: Ze3ter

#include "../../include/lex/file_manager.hpp"
#include "../../include/macros.hpp"
#include "../../include/runtime/allocator/arena.hpp"
#include "../../include/runtime/allocator/meta.hpp"
#include "../../include/types/string.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace mylang;

// ---------------------------------------------------------------------------
// V1 – Integer Overflow in updateNextBlockSize()
// ---------------------------------------------------------------------------

TEST(SecurityV1, MaxBlockSizeStillSafeForDoubling)
{
    constexpr size_t half_max = std::numeric_limits<size_t>::max() / 2;
    EXPECT_LT(static_cast<size_t>(MAX_BLOCK_SIZE), half_max)
        << "If MAX_BLOCK_SIZE exceeds SIZE_MAX/2, the unguarded current*2 will overflow";
}

TEST(SecurityV1, AllocateMustRejectOversizedRequest)
{
    runtime::allocator::ArenaAllocator alloc(
        static_cast<int>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR));
    // allocate() calls diagnostic::engine.emit(FATAL) which throws instead of returning nullptr.
    void* p = nullptr;
    EXPECT_ANY_THROW(p = alloc.allocate(MAX_BLOCK_SIZE + 1))
        << "allocate() must reject >MAX_BLOCK_SIZE (throws via diagnostic FATAL)";
    (void)p;
}

TEST(SecurityV1, ExponentialGrowthDoesNotCorruptMemory)
{
    runtime::allocator::ArenaAllocator alloc(
        static_cast<int>(runtime::allocator::ArenaAllocator::GrowthStrategy::EXPONENTIAL));
    for (int i = 0; i < 40; ++i) {
        void* p = alloc.allocate(512);
        if (!p)
            break;
        *static_cast<char*>(p) = static_cast<char>(i);
    }
    SUCCEED();
}

// ---------------------------------------------------------------------------
// V2 – Non-Atomic Reference Count
// ---------------------------------------------------------------------------

TEST(SecurityV2, SharedStringDataConfirmed)
{
    StringRef s1("shared");
    StringRef s2 = s1;
    EXPECT_EQ(s1.get(), s2.get()) << "Copy-construction shares String*; RefCount incremented non-atomically";
}

TEST(SecurityV2, ConcurrentCopyDestroyRace)
{
    StringRef shared("race target");
    std::atomic<bool> go { false };
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            while (!go.load(std::memory_order_acquire)) { }
            for (int i = 0; i < 500; ++i) {
                StringRef local = shared;
                (void)local.len();
            }
        });
    }
    go.store(true, std::memory_order_release);
    for (auto& th : threads)
        th.join();
    SUCCEED() << "Run with -fsanitize=thread to expose the data race on RefCount";
}

// ---------------------------------------------------------------------------
// V3 – Buffer Over-Read in operator==
// ---------------------------------------------------------------------------

TEST(SecurityV3, NormalAPICannotInflateLength)
{
    StringRef s1("SHORT"), s2("SHORT");
    EXPECT_EQ(s1, s2);
    EXPECT_GE(s1.cap(), s1.len()) << "cap() >= len() invariant holds; no over-read via normal API";
}

TEST(SecurityV3, LengthMismatchSkipsMemcmp)
{
    StringRef a("Hello"), b("Hi");
    EXPECT_NE(a, b);
    EXPECT_NE(a.len(), b.len()) << "Different lengths -> memcmp skipped entirely";
}

TEST(SecurityV3, CapInvariantSurvivesOperations)
{
    StringRef s("ABCDEFGHIJ");
    s += " extended";
    s.erase(0);
    s.truncate(5);
    EXPECT_GE(s.cap(), s.len());
}

// ---------------------------------------------------------------------------
// V4 – Dangling FileManager / raw null pointer
// ---------------------------------------------------------------------------

TEST(SecurityV4, TemporaryCopiedBeforeDestruction)
{
    StringRef captured;
    {
        std::string temp = "temporary data will be destroyed";
        captured = StringRef(temp.data());
    }
    EXPECT_EQ(captured, StringRef("temporary data will be destroyed"))
        << "StringRef copies data immediately; the .data()-of-temporary pattern is safe";
}

TEST(SecurityV4, FileManagerAcceptsValidPath)
{
    fs::path tmp = fs::temp_directory_path() / "v4_test.lang";
    {
        std::ofstream f(tmp);
        f << "hello";
    }
    ASSERT_NO_THROW({
        lex::FileManager fm(tmp.string());
        EXPECT_EQ(fm.buffer(), StringRef("hello"));
    });
    fs::remove(tmp);
}

TEST(SecurityV4, NullFileManagerPointerIsUndefinedBehaviour)
{
    lex::FileManager* raw = nullptr;
    EXPECT_EQ(raw, nullptr)
        << "Lexer(FileManager*) has no null check; passing nullptr is UB (CWE-476)";
}

// ---------------------------------------------------------------------------
// V5 – Path Traversal in FileManager::load()
// ---------------------------------------------------------------------------

class PathTraversalTest : public ::testing::Test {
protected:
    fs::path safe_dir_, safe_file_, outside_file_;

    void SetUp() override
    {
        safe_dir_ = fs::temp_directory_path() / "v5_safe";
        safe_file_ = safe_dir_ / "prog.lang";
        outside_file_ = fs::temp_directory_path() / "v5_secret.txt";
        fs::create_directories(safe_dir_);
        {
            std::ofstream f(safe_file_);
            f << "safe code";
        }
        {
            std::ofstream f(outside_file_);
            f << "SECRET DATA";
        }
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(safe_dir_, ec);
        fs::remove(outside_file_, ec);
    }
};

TEST_F(PathTraversalTest, RelativeTraversalEscapesSafeDir)
{
    lex::FileManager fm(safe_file_.string());
    fs::path traversal = safe_dir_ / ".." / "v5_secret.txt";
    StringRef result = fm.load(traversal.string());
    EXPECT_EQ(result, StringRef("SECRET DATA"))
        << "CONFIRMED: load() read a file outside the intended directory via ../";
}

TEST_F(PathTraversalTest, AbsolutePathOutsideSafeDirAccepted)
{
    lex::FileManager fm(safe_file_.string());
    StringRef result = fm.load(outside_file_.string());
    EXPECT_EQ(result, StringRef("SECRET DATA"))
        << "CONFIRMED: load() accepts any absolute path; no boundary check exists";
}

TEST_F(PathTraversalTest, SafeFileStillReadableAsBaseline)
{
    lex::FileManager fm(safe_file_.string());
    StringRef result = fm.load(safe_file_.string());
    EXPECT_EQ(result, StringRef("safe code"));
}

// ---------------------------------------------------------------------------
// V6 – expand() Dangling Pointer
// ---------------------------------------------------------------------------

TEST(SecurityV6, EnsureUniquePreventsDangle)
{
    StringRef str1("Hello"), str2 = str1;
    ASSERT_EQ(str1.get(), str2.get());
    str1.expand(1024);
    EXPECT_NE(str1.get(), str2.get()) << "expand() detached str1 via ensureUnique()";
    EXPECT_EQ(str2, StringRef("Hello")) << "str2 is NOT dangling — audit PoC is incorrect";
    EXPECT_GE(str1.cap(), 1024u);
}

TEST(SecurityV6, MultipleSharedRefsUnaffectedByExpand)
{
    StringRef orig("Original"), c1 = orig, c2 = orig;
    orig.expand(8192);
    EXPECT_EQ(c1, StringRef("Original"));
    EXPECT_EQ(c2, StringRef("Original"));
    EXPECT_NE(orig.get(), c1.get());
}

TEST(SecurityV6, SliceViewNotDangledByExpand)
{
    StringRef s("Hello World");
    StringRef view = s.slice(0, 4);
    s.expand(2048);
    EXPECT_EQ(view, StringRef("Hello")) << "Zero-copy slice is not dangling after expand()";
}

// ---------------------------------------------------------------------------
// V7 – uint32_t Truncation in AllocationHeader
// ---------------------------------------------------------------------------

TEST(SecurityV7, MaxBlockSizeExceedsUint32Max)
{
    constexpr uint32_t truncated = static_cast<uint32_t>(MAX_BLOCK_SIZE);
    EXPECT_GT(static_cast<size_t>(MAX_BLOCK_SIZE),
        static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
    EXPECT_EQ(truncated, 0u) << "static_cast<uint32_t>(MAX_BLOCK_SIZE) wraps to 0";
}

TEST(SecurityV7, CastIsLossyForSizesAboveUint32Max)
{
    size_t cases[] = {
        static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1ULL,
        static_cast<size_t>(MAX_BLOCK_SIZE),
    };
    for (size_t sz : cases) {
        uint32_t stored = static_cast<uint32_t>(sz);
        EXPECT_NE(static_cast<size_t>(stored), sz)
            << "Size " << sz << " truncated to " << stored;
    }
}

TEST(SecurityV7, CorruptedChecksumGoesUndetected)
{
    using namespace mylang::runtime::allocator;
    AllocationHeader h {};
    h.magic = AllocationHeader::MAGIC;
    h.alignment = static_cast<uint32_t>(alignof(std::max_align_t));
    h.size = static_cast<uint32_t>(MAX_BLOCK_SIZE); // truncates 4 GiB → 0
    h.checksum = h.compute_checksum();

    // The size field silently records 0 instead of 4 GiB
    EXPECT_EQ(h.size, 0u)
        << "size field is 0 after truncation; the real allocation was " << MAX_BLOCK_SIZE << " bytes";

    // is_valid() uses the already-corrupted size field, so it accepts the wrong metadata
    EXPECT_TRUE(h.is_valid())
        << "is_valid() returns true despite size=0: truncation is undetectable by the validator";

    // Confirm the stored size differs from the real allocation size
    EXPECT_NE(static_cast<size_t>(h.size), static_cast<size_t>(MAX_BLOCK_SIZE))
        << "AllocationHeader.size (0) != real allocation size (4 GiB): metadata is silently wrong";
}
