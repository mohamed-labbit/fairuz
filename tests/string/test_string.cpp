// stringref_test.cpp - Comprehensive brutal test suite for StringRef
#include "../../include/string.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

using namespace mylang;

// constructor

class StringRefTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Fresh allocator state for each test
    }

    void TearDown() override
    {
        // Clean up
    }
};

TEST_F(StringRefTest, SizeConstructor_Zero)
{
    StringRef s(static_cast<size_t>(0));
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
}

TEST_F(StringRefTest, SizeConstructor_SmallSize)
{
    StringRef s(10);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
    EXPECT_GE(s.cap(), 10);
}

TEST_F(StringRefTest, SizeConstructor_LargeSize)
{
    StringRef s(10000);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
    EXPECT_GE(s.cap(), 10000);
}

TEST_F(StringRefTest, CopyConstructor_Empty)
{
    StringRef s1;
    StringRef s2(s1);
    EXPECT_TRUE(s2.empty());
    EXPECT_EQ(s2.len(), 0);
}

TEST(DiagnosticTest, CopyConstructorDetailed)
{
    StringRef s1("Hell");
    StringRef s2(s1);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s1.len(), s2.len());
    s1 += 'H';

    EXPECT_NE(s1, s2);
    EXPECT_EQ(s1, StringRef("HellH"));
    EXPECT_EQ(s2, StringRef("Hell"));

    EXPECT_NE(s1.data(), s2.data());
}

TEST(DiagnosticTest, CopyConstructorArabic)
{
    StringRef s1("مرحبا");
    StringRef s2(s1);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s2, StringRef("مرحبا"));
}

TEST(DiagnosticTest, CheckMemoryIndependence)
{
    StringRef s1("Test");
    StringRef s2(s1);

    s1[0] = 'X'; // This triggers CoW

    char* ptr1 = s1.data();
    char* ptr2 = s2.data();

    EXPECT_NE(ptr1, ptr2) << "After CoW, should have different memory!";
    EXPECT_EQ(s1[0], 'X');
    EXPECT_EQ(s2[0], 'T');
    EXPECT_EQ(s1, "Xest");
    EXPECT_EQ(s2, "Test");
}

TEST_F(StringRefTest, CopyConstructor_NonEmpty)
{
    StringRef s1("Hello");
    StringRef s2(s1);
    EXPECT_EQ(s2.len(), s1.len());
    EXPECT_EQ(s2, s1);
}

TEST_F(StringRefTest, CopyConstructor_Arabic)
{
    StringRef s1("مرحبا");
    StringRef s2(s1);
    EXPECT_EQ(s2, s1);
    EXPECT_EQ(s2, "مرحبا");
}

TEST_F(StringRefTest, MoveConstructor_NonEmpty)
{
    StringRef s1("Hello");
    char const* old_ptr = s1.data();
    size_t old_len = s1.len();

    StringRef s2(std::move(s1));

    EXPECT_EQ(s2.len(), old_len);
    EXPECT_EQ(s2.data(), old_ptr);
    EXPECT_TRUE(s1.empty());
    EXPECT_EQ(s1.data(), nullptr);
}

TEST_F(StringRefTest, CStyleConstructor_Null)
{
    char const* null_ptr = nullptr;
    StringRef s(null_ptr);
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, CStyleConstructor_Empty)
{
    char empty[] = { char { 0 } };
    StringRef s(empty);
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, CStyleConstructor_NonEmpty)
{
    char hello[] = { 'H', 'e', 'l', 'l', 'o', char { 0 } };
    StringRef s(hello);
    EXPECT_EQ(s.len(), 5);
    EXPECT_EQ(s[0], 'H');
    EXPECT_EQ(s[4], 'o');
}

// assign

TEST_F(StringRefTest, CopyAssignment_SelfAssignment)
{
    StringRef s("Test");
    s = s; // Self-assignment
    EXPECT_EQ(s, "Test");
}

// ADDED: Test for copy assignment with shared data
TEST_F(StringRefTest, CopyAssignment_SharedData)
{
    StringRef s1("Hello");
    StringRef s2 = s1.slice(0, 2); // s1 and s2 share StringData_

    s1 = s2;
    EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, CopyAssignment_EmptyToEmpty)
{
    StringRef s1, s2;
    s2 = s1;
    EXPECT_TRUE(s2.empty());
}

TEST_F(StringRefTest, CopyAssignment_NonEmptyToEmpty)
{
    StringRef s1("Hello");
    StringRef s2;
    s2 = s1;
    EXPECT_EQ(s2, s1);
}

TEST_F(StringRefTest, CopyAssignment_EmptyToNonEmpty)
{
    StringRef s1("Hello");
    StringRef s2;
    s1 = s2;
    EXPECT_TRUE(s1.empty());
}

TEST_F(StringRefTest, CopyAssignment_ReplaceContent)
{
    StringRef s1("Hello");
    StringRef s2("World");
    s1 = s2;
    EXPECT_EQ(s1, "World");
}

TEST_F(StringRefTest, MoveAssignment_SelfAssignment)
{
    StringRef s("Test");
    s = std::move(s); // Self-assignment
    EXPECT_EQ(s, "Test");
}

TEST_F(StringRefTest, MoveAssignment_NonEmpty)
{
    StringRef s1("Hello");
    StringRef s2("World");
    char const* old_ptr = s2.data();

    s1 = std::move(s2);

    EXPECT_EQ(s1.data(), old_ptr);
    EXPECT_EQ(s1, "World");
    EXPECT_TRUE(s2.empty());
}

TEST_F(StringRefTest, Utf8Assignment_FromCString)
{
    StringRef s;
    s = "Hello";
    EXPECT_EQ(s, "Hello");
}

TEST_F(StringRefTest, Utf8Assignment_NullPtr)
{
    StringRef s("Hello");
    s = static_cast<char const*>(nullptr);
    EXPECT_TRUE(s.empty());
}

// comparison

TEST_F(StringRefTest, Equality_BothEmpty)
{
    StringRef s1, s2;
    EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_SameContent)
{
    StringRef s1("Hello");
    StringRef s2("Hello");
    EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_DifferentContent)
{
    StringRef s1("Hello");
    StringRef s2("World");
    EXPECT_NE(s1, s2);
}

TEST_F(StringRefTest, Equality_DifferentLength)
{
    StringRef s1("Hello");
    StringRef s2("Hi");
    EXPECT_NE(s1, s2);
}

TEST_F(StringRefTest, Equality_SelfComparison)
{
    StringRef s("Test");
    EXPECT_EQ(s, s);
}

TEST_F(StringRefTest, Equality_ArabicText)
{
    StringRef s1("مرحبا بك");
    StringRef s2("مرحبا بك");
    EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_ChineseText)
{
    StringRef s1("你好世界");
    StringRef s2("你好世界");
    EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_Emoji)
{
    StringRef s1("Hello 😀🎉");
    StringRef s2("Hello 😀🎉");
    EXPECT_EQ(s1, s2);
}

// memory

TEST_F(StringRefTest, Expand_FromEmpty)
{
    StringRef s;
    s.expand(100);
    EXPECT_GE(s.cap(), 100);
    EXPECT_EQ(s.len(), 0);
}

TEST_F(StringRefTest, Expand_AlreadyLargeEnough)
{
    StringRef s(100);
    size_t old_cap = s.cap();
    s.expand(50);
    EXPECT_EQ(s.cap(), old_cap); // Should not change
}

TEST_F(StringRefTest, Expand_GrowthFactor)
{
    StringRef s(10);
    s.expand(100);
    // Should allocate more than 100 due to 1.5x growth factor
    EXPECT_GE(s.cap(), 100);
}

TEST_F(StringRefTest, Reserve_IncreasesCapacity)
{
    StringRef s;
    s.reserve(1000);
    EXPECT_GE(s.cap(), 1000);
    EXPECT_EQ(s.len(), 0);
}

TEST_F(StringRefTest, Reserve_PreservesContent)
{
    StringRef s("Hello");
    s.reserve(1000);
    EXPECT_EQ(s, "Hello");
    EXPECT_GE(s.cap(), 1000);
}

TEST_F(StringRefTest, Clear_EmptiesString)
{
    StringRef s("Hello");
    s.clear();
    EXPECT_EQ(s.len(), 0);
    EXPECT_TRUE(s.empty());
    EXPECT_GT(s.cap(), 0); // Capacity should remain
}

TEST_F(StringRefTest, Clear_MultipleTime)
{
    StringRef s("Hello");
    s.clear();
    s.clear();
    s.clear();
    EXPECT_EQ(s.len(), 0);
}

TEST_F(StringRefTest, Resize_IncreasesCapacity)
{
    StringRef s;
    s.resize(500);
    EXPECT_GE(s.cap(), 500);
}

// append

TEST_F(StringRefTest, AppendChar_ToEmpty)
{
    StringRef s;
    s += 'A';
    EXPECT_EQ(s.len(), 1);
    EXPECT_EQ(s[0], 'A');
}

TEST_F(StringRefTest, AppendChar_Multiple)
{
    StringRef s;
    s += 'H';
    s += 'i';
    EXPECT_EQ(s.len(), 2);
    EXPECT_EQ(s, "Hi");
}

TEST_F(StringRefTest, AppendChar_UnicodeChar)
{
    StringRef s;
    s += char(0x0645);
    EXPECT_GE(s.len(), 1);
}

TEST_F(StringRefTest, AppendChar_TriggerExpansion)
{
    StringRef s(2);
    for (int i = 0; i < 100; i++)
        s += 'A';
    EXPECT_EQ(s.len(), 100);
}

TEST_F(StringRefTest, AppendString_ToEmpty)
{
    StringRef s1;
    StringRef s2("Hello");
    s1 += s2;
    EXPECT_EQ(s1, "Hello");
}

TEST_F(StringRefTest, AppendString_EmptyToNonEmpty)
{
    StringRef s1("Hello");
    StringRef s2;
    s1 += s2;
    EXPECT_EQ(s1, "Hello");
}

TEST_F(StringRefTest, AppendString_NonEmptyToNonEmpty)
{
    StringRef s1("Hello");
    StringRef s2(" World");
    s1 += s2;
    EXPECT_EQ(s1, "Hello World");
}

TEST_F(StringRefTest, AppendString_Arabic)
{
    StringRef s1("مرحبا");
    StringRef s2(" بك");
    s1 += s2;
    EXPECT_EQ(s1, "مرحبا بك");
}

TEST_F(StringRefTest, AppendString_TriggerReallocation)
{
    StringRef s1(5);
    StringRef s2("This is a very long string that will trigger reallocation");
    s1 += s2;
    EXPECT_EQ(s1, s2);
}

// concatenation

TEST_F(StringRefTest, Concat_StringPlusString)
{
    StringRef s1("Hello");
    StringRef s2(" World");
    StringRef s3 = s1 + s2;
    EXPECT_EQ(s3, "Hello World");
}

TEST_F(StringRefTest, Concat_EmptyPlusEmpty)
{
    StringRef s1, s2;
    StringRef s3 = s1 + s2;
    EXPECT_TRUE(s3.empty());
}

TEST_F(StringRefTest, Concat_EmptyPlusNonEmpty)
{
    StringRef s1;
    StringRef s2("Hello");
    StringRef s3 = s1 + s2;
    EXPECT_EQ(s3, "Hello");
}

TEST_F(StringRefTest, Concat_StringPlusCString)
{
    StringRef s1("Hello");
    StringRef s2 = s1 + " World";
    EXPECT_EQ(s2, "Hello World");
}

TEST_F(StringRefTest, Concat_CStringPlusString)
{
    StringRef s1(" World");
    StringRef s2 = "Hello" + s1;
    EXPECT_EQ(s2, "Hello World");
}

TEST_F(StringRefTest, Concat_StringPlusChar)
{
    StringRef s1("Hell");
    StringRef s2 = s1 + char('o');
    EXPECT_EQ(s2, "Hello");
}

TEST_F(StringRefTest, Concat_CharPlusString)
{
    StringRef s1("ello");
    StringRef s2 = char('H') + s1;
    EXPECT_EQ(s2, "Hello");
}

TEST_F(StringRefTest, Concat_Chain)
{
    StringRef s1("A");
    StringRef s2("B");
    StringRef s3("C");
    StringRef result = s1 + s2 + s3;
    EXPECT_EQ(result, "ABC");
}

TEST_F(StringRefTest, Concat_NullCString)
{
    StringRef s1("Hello");
    StringRef s2 = s1 + static_cast<char const*>(nullptr);
    EXPECT_EQ(s2, "Hello");
}

// index access

TEST_F(StringRefTest, IndexOperator_ValidAccess)
{
    StringRef s("Hello");
    EXPECT_EQ(s[0], 'H');
    EXPECT_EQ(s[4], 'o');
}

TEST_F(StringRefTest, IndexOperator_Mutable)
{
    StringRef s("Hello");
    s[0] = 'J';
    EXPECT_EQ(s[0], 'J');
}

TEST_F(StringRefTest, At_ValidAccess)
{
    StringRef s("Hello");
    EXPECT_EQ(s.at(0), 'H');
    EXPECT_EQ(s.at(4), 'o');
}

TEST_F(StringRefTest, At_OutOfBounds)
{
    StringRef s("Hi");
    EXPECT_THROW(s.at(10), std::out_of_range);
}

TEST_F(StringRefTest, At_MutableAccess)
{
    StringRef s("Hello");
    s.at(0) = 'J';
    EXPECT_EQ(s.at(0), 'J');
}

TEST_F(StringRefTest, At_EmptyString)
{
    StringRef s;
    EXPECT_THROW(s.at(0), std::out_of_range);
}

// erase

TEST_F(StringRefTest, Erase_FirstChar)
{
    StringRef s("Hello");
    s.erase(0);
    EXPECT_EQ(s, "ello");
}

TEST_F(StringRefTest, Erase_LastChar)
{
    StringRef s("Hello");
    s.erase(4);
    EXPECT_EQ(s, "Hell");
}

TEST_F(StringRefTest, Erase_MiddleChar)
{
    StringRef s("Hello");
    s.erase(2);
    EXPECT_EQ(s, "Helo");
}

TEST_F(StringRefTest, Erase_OutOfBounds)
{
    StringRef s("Hello");
    s.erase(100); // Should do nothing
    EXPECT_EQ(s, "Hello");
}

TEST_F(StringRefTest, Erase_EmptyString)
{
    StringRef s;
    s.erase(0); // Should do nothing
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Erase_AllChars)
{
    StringRef s("Hi");
    s.erase(0);
    s.erase(0);
    EXPECT_TRUE(s.empty());
}

// find

TEST_F(StringRefTest, Find_CharExists)
{
    StringRef s("Hello");
    EXPECT_TRUE(s.find('H'));
    EXPECT_TRUE(s.find('o'));
}

TEST_F(StringRefTest, Find_CharNotExists)
{
    StringRef s("Hello");
    EXPECT_FALSE(s.find('X'));
}

TEST_F(StringRefTest, Find_EmptyString)
{
    StringRef s;
    EXPECT_FALSE(s.find('A'));
}

TEST_F(StringRefTest, FindPos_CharExists)
{
    StringRef s("Hello");
    auto pos = s.find_pos('e');
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 1);
}

TEST_F(StringRefTest, FindPos_CharNotExists)
{
    StringRef s("Hello");
    auto pos = s.find_pos('X');
    EXPECT_FALSE(pos.has_value());
}

TEST_F(StringRefTest, FindPos_FirstOccurrence)
{
    StringRef s("Hello");
    auto pos = s.find_pos('l');
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 2); // First 'l'
}

// truncate

TEST_F(StringRefTest, Truncate_ToShorter)
{
    StringRef s("Hello");
    s.truncate(3);
    EXPECT_EQ(s.len(), 3);
    EXPECT_EQ(s, "Hel");
}

TEST_F(StringRefTest, Truncate_ToZero)
{
    StringRef s("Hello");
    s.truncate(0);
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Truncate_ToLargerSize)
{
    StringRef s("Hello");
    s.truncate(100); // Should do nothing
    EXPECT_EQ(s, "Hello");
}

TEST_F(StringRefTest, Truncate_EmptyString)
{
    StringRef s;
    s.truncate(5); // Should do nothing
    EXPECT_TRUE(s.empty());
}

// substring - FIXED TESTS

TEST_F(StringRefTest, Substr_FullString)
{
    StringRef s("Hello");
    StringRef sub = s.substr(0, 5);
    // FIXED: substr(0, 4) with inclusive end should return "Hello" (5 chars)
    EXPECT_EQ(sub, "Hello");
}

TEST_F(StringRefTest, Substr_MiddlePortion)
{
    StringRef s("Hello World");
    StringRef sub = s.substr(6, 11);
    // FIXED: This should return "World" (indices 6-10 inclusive)
    EXPECT_EQ(sub, "World");
}

TEST_F(StringRefTest, Substr_FirstChar)
{
    StringRef s("Hello");
    StringRef sub = s.substr(0, 1);
    EXPECT_EQ(sub.len(), 1);
    EXPECT_EQ(sub[0], 'H');
}

TEST_F(StringRefTest, Substr_LastChar)
{
    StringRef s("Hello");
    StringRef sub = s.substr(4, 5);
    EXPECT_EQ(sub.len(), 1);
    EXPECT_EQ(sub[0], 'o');
}

TEST_F(StringRefTest, Substr_DefaultEnd)
{
    StringRef s("Hello");
    StringRef sub = s.substr(2);
    EXPECT_EQ(sub, "llo");
}

TEST_F(StringRefTest, Substr_EmptyString)
{
    StringRef s;
    StringRef sub = s.substr(0, 1);
    EXPECT_TRUE(sub.empty());
}

TEST_F(StringRefTest, Substr_StartOutOfBounds)
{
    StringRef s("Hello");
    EXPECT_THROW(s.substr(100, 101), std::out_of_range);
}

TEST_F(StringRefTest, Substr_EndLargerThanLength)
{
    StringRef s("Hello");
    StringRef sub = s.substr(0, 101); // Should clamp to length
    EXPECT_EQ(sub, "Hello");
}

TEST_F(StringRefTest, Substr_EndBeforeStart)
{
    StringRef s("Hello");
    EXPECT_THROW(s.substr(4, 3), std::invalid_argument);
}

TEST_F(StringRefTest, Substr_ArabicText)
{
    StringRef s("مرحبا بك في العالم");
    StringRef sub = s.substr(0, 10); // each char is 2 bytes there
    EXPECT_EQ(sub, "مرحبا");
}

// ADDED: slice tests
TEST_F(StringRefTest, Slice_FullString)
{
    StringRef s("Hello");
    StringRef sliced = s.slice(0, 4); // Should be indices [0, 4] inclusive
    EXPECT_EQ(sliced, "Hello");
}

TEST_F(StringRefTest, Slice_ChainedSlices)
{
    StringRef s("Hello World");
    StringRef s1 = s.slice(0, 4);  // "Hello"
    StringRef s2 = s1.slice(1, 3); // "ell"
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "ell");
}

TEST_F(StringRefTest, Slice_SharesMemory)
{
    StringRef s1("Hello");
    StringRef s2 = s1.slice(0, 2); // "Hel"

    // They should share the underlying StringData_
    EXPECT_EQ(s1.get(), s2.get());

    // Modifying s2 should trigger CoW
    s2[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "Jel");
}

// UTF-8 conversion

TEST_F(StringRefTest, FromUtf8_EmptyString)
{
    StringRef s("");
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, FromUtf8_AsciiString)
{
    StringRef s("Hello World");
    EXPECT_EQ(s, "Hello World");
}

TEST_F(StringRefTest, FromUtf8_ArabicString)
{
    StringRef s("مرحبا");
    EXPECT_EQ(s, "مرحبا");
}

TEST_F(StringRefTest, FromUtf8_ChineseString)
{
    StringRef s("你好世界");
    EXPECT_EQ(s, "你好世界");
}

TEST_F(StringRefTest, FromUtf8_JapaneseString)
{
    StringRef s("こんにちは");
    EXPECT_EQ(s, "こんにちは");
}

TEST_F(StringRefTest, FromUtf8_RussianString)
{
    StringRef s("Привет мир");
    EXPECT_EQ(s, "Привет мир");
}

TEST_F(StringRefTest, FromUtf8_EmojiString)
{
    StringRef s("Hello 😀 World 🎉");
    EXPECT_EQ(s, "Hello 😀 World 🎉");
}

TEST_F(StringRefTest, FromUtf8_MixedScript)
{
    StringRef s("Hello مرحبا 你好 🌍");
    EXPECT_EQ(s, "Hello مرحبا 你好 🌍");
}

TEST_F(StringRefTest, FromUtf8_NullPtr)
{
    StringRef s(static_cast<char const*>(nullptr));
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, ToUtf8_EmptyString)
{
    StringRef s;
    EXPECT_EQ(s, "");
}

TEST_F(StringRefTest, ToUtf8_RoundTrip)
{
    std::string original = "Hello World مرحبا 你好 😀";
    StringRef s(original.data());
    std::string result = s.data();
    EXPECT_EQ(result, original);
}

// to double

TEST_F(StringRefTest, ToDouble_Integer)
{
    StringRef s("42");
    EXPECT_DOUBLE_EQ(s.toDouble(), 42.0);
}

TEST_F(StringRefTest, ToDouble_Float)
{
    StringRef s("3.14159");
    EXPECT_DOUBLE_EQ(s.toDouble(), 3.14159);
}

TEST_F(StringRefTest, ToDouble_Negative)
{
    StringRef s("-123.456");
    EXPECT_DOUBLE_EQ(s.toDouble(), -123.456);
}

TEST_F(StringRefTest, ToDouble_Scientific)
{
    StringRef s("1.5e10");
    EXPECT_DOUBLE_EQ(s.toDouble(), 1.5e10);
}

TEST_F(StringRefTest, ToDouble_EmptyString)
{
    StringRef s;
    EXPECT_THROW(s.toDouble(), std::invalid_argument);
}

TEST_F(StringRefTest, ToDouble_InvalidFormat)
{
    StringRef s("not a number");
    EXPECT_THROW(s.toDouble(), std::invalid_argument);
}

TEST_F(StringRefTest, ToDouble_WithPosition)
{
    StringRef s("123.456abc");
    size_t pos = 0;
    double result = s.toDouble(&pos);
    EXPECT_DOUBLE_EQ(result, 123.456);
    EXPECT_GT(pos, 0);
}

// hash

TEST_F(StringRefTest, Hash_EmptyString)
{
    StringRefHash hasher;
    StringRef s;
    EXPECT_EQ(hasher(s), 0);
}

TEST_F(StringRefTest, Hash_NonEmpty)
{
    StringRefHash hasher;
    StringRef s("Hello");
    size_t hash = hasher(s);
    EXPECT_NE(hash, 0);
}

TEST_F(StringRefTest, Hash_SameContent)
{
    StringRefHash hasher;
    StringRef s1("Hello");
    StringRef s2("Hello");
    EXPECT_EQ(hasher(s1), hasher(s2));
}

TEST_F(StringRefTest, Hash_DifferentContent)
{
    StringRefHash hasher;
    StringRef s1("Hello");
    StringRef s2("World");
    EXPECT_NE(hasher(s1), hasher(s2));
}

TEST_F(StringRefTest, StdHash_Works)
{
    std::hash<StringRef> hasher;
    StringRef s("Test");
    size_t hash = hasher(s);
    EXPECT_NE(hash, 0);
}

// stress tests

TEST_F(StringRefTest, Stress_ManyAppends)
{
    StringRef s;
    for (int i = 0; i < 10000; i++)
        s += char('A' + (i % 26));
    EXPECT_EQ(s.len(), 10000);
}

TEST_F(StringRefTest, Stress_ManyErases)
{
    StringRef s;
    for (int i = 0; i < 1000; i++)
        s += char('A');
    for (int i = 0; i < 500; i++)
        s.erase(0);
    EXPECT_EQ(s.len(), 500);
}

TEST_F(StringRefTest, Stress_CopyAndModify)
{
    StringRef original("Original");
    std::vector<StringRef> copies;

    for (int i = 0; i < 100; i++) {
        copies.push_back(original);
        copies.back() += char('0' + i % 10);
    }

    EXPECT_EQ(original, "Original");
    for (size_t i = 0; i < copies.size(); i++)
        EXPECT_NE(copies[i], original);
}

TEST_F(StringRefTest, Stress_LargeString)
{
    std::string large(100000, 'A');
    StringRef s(large.data());
    EXPECT_EQ(s.len(), 100000);
    // EXPECT_EQ(s, large);
}

TEST_F(StringRefTest, Stress_UnicodeAppends)
{
    StringRef s;
    for (int i = 0; i < 1000; i++)
        s = s + "مرحبا";

    EXPECT_EQ(s.len(), 10000); // each char in 2 bytes
}

TEST_F(StringRefTest, Stress_ManySubstrings)
{
    StringRef s("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    std::vector<StringRef> subs;

    for (size_t i = 0; i < s.len(); i++)
        for (size_t j = i; j < s.len(); j++)
            subs.push_back(s.substr(i, j));

    EXPECT_GT(subs.size(), 100);
}

TEST_F(StringRefTest, Stress_RandomOperations)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<> op_dist(0, 4);
    std::uniform_int_distribution<> char_dist('A', 'Z');

    StringRef s;

    for (int i = 0; i < 1000; i++) {
        int op = op_dist(rng);
        switch (op) {
        case 0: // Append
            s += char(char_dist(rng));
            break;
        case 1: // Clear
            s.clear();
            break;
        case 2: // Erase
            if (!s.empty())
                s.erase(0);
            break;
        case 3: // Truncate
            if (!s.empty())
                s.truncate(s.len() / 2);
            break;
        case 4: // Copy
        {
            StringRef copy = s;
            s = copy;
        } break;
        }
    }

    // Should not crash
    EXPECT_TRUE(true);
}

// edge cases

TEST_F(StringRefTest, EdgeCase_NullTerminatorInMiddle)
{
    StringRef s("Hello");
    s[2] = char { 0 };
    // toUtf8 should handle this correctly (might stop at null or not)
    EXPECT_LE(s.len(), 5);
}

TEST_F(StringRefTest, EdgeCase_MaxSizeString)
{
    // Test with a reasonably large string
    size_t const large_size = 1000000;
    StringRef s(large_size);
    for (size_t i = 0; i < 100; i++)
        s += 'A';
    EXPECT_EQ(s.len(), 100);
}

TEST_F(StringRefTest, EdgeCase_EmptyOperations)
{
    StringRef s;
    s += StringRef();
    s.clear();
    s.erase(0);
    s.truncate(0);
    auto sub = s.substr(0);
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(sub.empty());
}

TEST_F(StringRefTest, EdgeCase_SurrogatesPairs)
{
    // Test emoji with surrogate pairs
    StringRef s("😀😃😄😁");
    EXPECT_GT(s.len(), 4); // Each emoji is multiple UTF-8 bytes
}

TEST_F(StringRefTest, EdgeCase_AllZeros)
{
    StringRef s(10);
    for (int i = 0; i < 10; i++)
        s += char { 0 };
    EXPECT_EQ(s.len(), 10);
}

TEST_F(StringRefTest, EdgeCase_HighUnicodeValues)
{
    // FIXED: This test doesn't make sense for char-based strings
    // char(0xD800) is not valid for char
    // Skip this test or adjust for UTF-8
    StringRef s("𝕳𝖊𝖑𝖑𝖔");  // Mathematical bold text
    EXPECT_GT(s.len(), 5); // Multi-byte UTF-8
}

// memory leaks

TEST_F(StringRefTest, NoLeak_MultipleConstructDestruct)
{
    for (int i = 0; i < 1000; i++)
        StringRef s("Test String");
    // Valgrind or AddressSanitizer will catch leaks
    EXPECT_TRUE(true);
}

TEST_F(StringRefTest, NoLeak_CopyAssignmentLoop)
{
    StringRef original("Original");
    for (int i = 0; i < 100; i++) {
        StringRef copy;
        copy = original;
    }
    EXPECT_TRUE(true);
}

TEST_F(StringRefTest, NoLeak_MoveAssignmentLoop)
{
    for (int i = 0; i < 100; i++) {
        StringRef s1("Test");
        StringRef s2 = std::move(s1);
    }
    EXPECT_TRUE(true);
}

// perf

TEST_F(StringRefTest, Performance_AppendChars)
{
    auto start = std::chrono::high_resolution_clock::now();

    StringRef s;
    s.reserve(10000);
    for (int i = 0; i < 10000; i++)
        s += char('A');

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(s.len(), 10000);
    std::cout << "Append 10000 chars took: " << duration.count() << "ms\n";
}

TEST_F(StringRefTest, Performance_Concatenation)
{
    auto start = std::chrono::high_resolution_clock::now();

    StringRef result;
    StringRef part("Part");

    for (int i = 0; i < 1000; i++)
        result = result + part;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.len(), 4000);
    std::cout << "1000 concatenations took: " << duration.count() << "ms\n";
}

TEST_F(StringRefTest, Performance_Utf8Conversion)
{
    std::string utf8(10000, 'A');

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        StringRef s(utf8.data());
        std::string back = s.data();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 UTF-8 round-trips (10k chars) took: " << duration.count() << "ms\n";
}

TEST_F(StringRefTest, TestTrim)
{
    StringRef s1 = "abc   ";
    StringRef s2 = "   abc";
    StringRef s3 = "   abc    ";

    s1.trimWhitespace();
    s2.trimWhitespace();
    s3.trimWhitespace();

    EXPECT_EQ(s1, StringRef("abc"));
    EXPECT_EQ(s2, StringRef("abc"));
    EXPECT_EQ(s3, StringRef("abc"));
}

// ADDED: CoW-specific tests
TEST_F(StringRefTest, CoW_BasicSharing)
{
    StringRef s1("Hello");
    StringRef s2 = s1;

    // They should share the same underlying data
    EXPECT_EQ(s1.get(), s2.get());

    // Modification triggers CoW
    s1[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Jello");
    EXPECT_EQ(s2, "Hello");
}

TEST_F(StringRefTest, CoW_NonConstData)
{
    StringRef s1("Test");
    StringRef s2 = s1;

    // Calling non-const data() should trigger CoW
    char* ptr1 = s1.data();
    char const* ptr2 = s2.data();

    EXPECT_NE(ptr1, ptr2);
}
