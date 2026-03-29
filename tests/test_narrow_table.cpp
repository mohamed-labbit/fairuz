#include "../include/table.hpp"

#include <gtest/gtest.h>

using namespace fairuz;

namespace {

struct IntHash {
    size_t operator()(int value) const
    {
        return static_cast<size_t>(static_cast<u32>(value) * 2654435761u);
    }
};

struct IntEqual {
    bool operator()(int lhs, int rhs) const
    {
        return lhs == rhs;
    }
};

struct CollidingHash {
    size_t operator()(int) const
    {
        return 1;
    }
};

using IntTable = Fa_HashTable<int, int, IntHash, IntEqual>;
using CollidingIntTable = Fa_HashTable<int, int, CollidingHash, IntEqual>;

} // namespace

TEST(Fa_HashTable, InsertAndFindSingleEntry)
{
    IntTable table;

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.findPtr(7), nullptr);

    int& stored = table.insertOrAssign(7, 99);
    EXPECT_EQ(stored, 99);
    EXPECT_EQ(table.size(), 1u);
    ASSERT_NE(table.findPtr(7), nullptr);
    EXPECT_EQ(*table.findPtr(7), 99);
    EXPECT_FALSE(table.empty());
}

TEST(Fa_HashTable, OverwriteKeepsSizeStable)
{
    IntTable table;

    table.insertOrAssign(3, 10);
    table.insertOrAssign(3, 42);

    EXPECT_EQ(table.size(), 1u);
    ASSERT_NE(table.findPtr(3), nullptr);
    EXPECT_EQ(*table.findPtr(3), 42);
}

TEST(Fa_HashTable, HandlesLinearProbingCollisions)
{
    CollidingIntTable table;

    for (int i = 0; i < 32; ++i)
        table.insertOrAssign(i, i * 10);

    EXPECT_EQ(table.size(), 32u);
    for (int i = 0; i < 32; ++i) {
        int* value = table.findPtr(i);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i * 10);
    }
}

TEST(Fa_HashTable, GrowthPreservesExistingEntries)
{
    IntTable table;

    for (int i = 0; i < 1000; ++i)
        table.insertOrAssign(i, i + 1);

    EXPECT_EQ(table.size(), 1000u);
    for (int i = 0; i < 1000; ++i) {
        int* value = table.findPtr(i);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i + 1);
    }
}

TEST(Fa_HashTable, ClearRemovesEntriesAndAllowsReuse)
{
    IntTable table;

    for (int i = 0; i < 64; ++i)
        table.insertOrAssign(i, i);

    table.clear();

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.size(), 0u);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(table.findPtr(i), nullptr);

    table.insertOrAssign(123, 456);
    ASSERT_NE(table.findPtr(123), nullptr);
    EXPECT_EQ(*table.findPtr(123), 456);
    EXPECT_EQ(table.size(), 1u);
}
