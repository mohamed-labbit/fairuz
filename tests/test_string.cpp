#include "../include/string.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

using namespace mylang;

class StringRefTest : public ::testing::Test {
protected:
    void SetUp() override { }

    void TearDown() override { }
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

TEST_F(StringRefTest, CopyConstructorDetailed)
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

TEST_F(StringRefTest, CopyConstructorArabic)
{
    StringRef s1("مرحبا");
    StringRef s2(s1);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s2, StringRef("مرحبا"));
}

TEST_F(StringRefTest, CheckMemoryIndependence)
{
    StringRef s1("Test");
    StringRef s2(s1);

    s1[0] = 'X';

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

TEST_F(StringRefTest, CopyAssignment_SelfAssignment)
{
    StringRef s("Test");
    s = s;
    EXPECT_EQ(s, "Test");
}

TEST_F(StringRefTest, CopyAssignment_SharedData)
{
    StringRef s1("Hello");
    StringRef s2 = s1.slice(0, 2);

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
    s = std::move(s);
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
    EXPECT_EQ(s.cap(), old_cap);
}

TEST_F(StringRefTest, Expand_GrowthFactor)
{
    StringRef s(10);
    s.expand(100);
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
    EXPECT_GT(s.cap(), 0);
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
    s.erase(100);
    EXPECT_EQ(s, "Hello");
}

TEST_F(StringRefTest, Erase_EmptyString)
{
    StringRef s;
    s.erase(0);
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Erase_AllChars)
{
    StringRef s("Hi");
    s.erase(0);
    s.erase(0);
    EXPECT_TRUE(s.empty());
}

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
    EXPECT_EQ(pos.value(), 2);
}

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
    s.truncate(100);
    EXPECT_EQ(s, "Hello");
}

TEST_F(StringRefTest, Truncate_EmptyString)
{
    StringRef s;
    s.truncate(5);
    EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Substr_FullString)
{
    StringRef s("Hello");
    StringRef sub = s.substr(0, 5);
    EXPECT_EQ(sub, "Hello");
}

TEST_F(StringRefTest, Substr_MiddlePortion)
{
    StringRef s("Hello World");
    StringRef sub = s.substr(6, 11);
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
    EXPECT_EQ(s.substr(100, 101), StringRef { });
}

TEST_F(StringRefTest, Substr_EndLargerThanLength)
{
    StringRef s("Hello");
    StringRef sub = s.substr(0, 101);
    EXPECT_EQ(sub, "Hello");
}

TEST_F(StringRefTest, Substr_EndBeforeStart)
{
    StringRef s("Hello");
    EXPECT_EQ(s.substr(4, 3), StringRef { });
}

TEST_F(StringRefTest, Substr_ArabicText)
{
    StringRef s("مرحبا بك في العالم");
    StringRef sub = s.substr(0, 10);
    EXPECT_EQ(sub, "مرحبا");
}

// ADDED: slice tests
TEST_F(StringRefTest, Slice_FullString)
{
    StringRef s("Hello");
    StringRef sliced = s.slice(0, 5);
    EXPECT_EQ(sliced, "Hello");
}

TEST_F(StringRefTest, Slice_ChainedSlices)
{
    StringRef s("Hello World");
    StringRef s1 = s.slice(0, 5);
    StringRef s2 = s1.slice(1, 4);
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "ell");
}

TEST_F(StringRefTest, Slice_SharesMemory)
{
    StringRef s1("Hello");
    StringRef s2 = s1.slice(0, 3);

    EXPECT_EQ(s1.get(), s2.get());

    s2[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "Jel");
}

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
}

TEST_F(StringRefTest, Stress_UnicodeAppends)
{
    StringRef s;
    for (int i = 0; i < 1000; i++)
        s = s + "مرحبا";

    EXPECT_EQ(s.len(), 10000);
}

TEST_F(StringRefTest, Stress_ManySubstrings)
{
    StringRef s("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    std::vector<StringRef> subs;

    for (size_t i = 0; i < s.len(); i++) {
        for (size_t j = i; j < s.len(); j++)
            subs.push_back(s.substr(i, j));
    }

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
        case 0:
            s += char(char_dist(rng));
            break;
        case 1:
            s.clear();
            break;
        case 2:
            if (!s.empty())
                s.erase(0);
            break;
        case 3:
            if (!s.empty())
                s.truncate(s.len() / 2);
            break;
        case 4: {
            StringRef copy = s;
            s = copy;
        } break;
        }
    }

    EXPECT_TRUE(true);
}

TEST_F(StringRefTest, EdgeCase_NullTerminatorInMiddle)
{
    StringRef s("Hello");
    s[2] = char { 0 };
    EXPECT_LE(s.len(), 5);
}

TEST_F(StringRefTest, EdgeCase_MaxSizeString)
{
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
    StringRef s("😀😃😄😁");
    EXPECT_GT(s.len(), 4);
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
    StringRef s("𝕳𝖊𝖑𝖑𝖔");
    EXPECT_GT(s.len(), 5);
}

TEST_F(StringRefTest, NoLeak_MultipleConstructDestruct)
{
    for (int i = 0; i < 1000; i++)
        StringRef s("Test String");
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

TEST_F(StringRefTest, CoW_BasicSharing)
{
    StringRef s1("Hello");
    StringRef s2 = s1;

    EXPECT_EQ(s1.get(), s2.get());

    s1[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Jello");
    EXPECT_EQ(s2, "Hello");
}

TEST_F(StringRefTest, CoW_NonConstData)
{
    StringRef s1("Test");
    StringRef s2 = s1;

    char* ptr1 = s1.data();
    char const* ptr2 = s2.data();

    EXPECT_NE(ptr1, ptr2);
}

static double microseconds_since(std::chrono::high_resolution_clock::time_point t0)
{
    using namespace std::chrono;
    return static_cast<double>(
               duration_cast<nanoseconds>(high_resolution_clock::now() - t0).count())
        / 1000.0;
}

// Prevent the compiler from optimizing away a value entirely.
// We write through a volatile sink so the read of `v` is observable.
template<typename T>
static void do_not_optimize(T const& v)
{
    void const volatile* sink = &v; // NOLINT
    (void)sink;
}

// ---------------------------------------------------------------------------
// Append throughput
// ---------------------------------------------------------------------------

// Baseline: 1 M single-char appends with pre-reserved buffer (pure write path).
TEST_F(StringRefTest, Append_1M_Chars_PreReserved)
{
    constexpr int N = 1'000'000;

    StringRef s;
    s.reserve(N);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        s += 'A';
    double us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N));
    std::printf("  Append 1M chars (pre-reserved):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Append with no pre-reservation — measures reallocation overhead.
TEST_F(StringRefTest, Append_1M_Chars_NoReserve)
{
    constexpr int N = 1'000'000;

    StringRef s;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        s += char('A' + i % 26);
    double us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N));
    std::printf("  Append 1M chars (no reserve):    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Append short string chunks — 250k * 4 bytes = 1 MB total.
TEST_F(StringRefTest, Append_250k_ShortStrings)
{
    constexpr int N = 250'000;
    StringRef chunk("abcd");

    StringRef s;
    s.reserve(N * 4);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        s += chunk;
    double us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N * 4));
    std::printf("  Append 250k short strings:       %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// Concatenation (operator+) — should be O(n) total not O(n²)
// ---------------------------------------------------------------------------

// 10k concatenations via operator+ building a growing string.
// If operator+ is naive this is O(n²); a good impl should stay linear-ish.
TEST_F(StringRefTest, Concat_10k_Growing)
{
    constexpr int N = 10'000;
    StringRef part("X");
    StringRef result;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        result = result + part;
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.len(), static_cast<size_t>(N));
    std::printf("  Concat 10k growing (operator+):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Same total bytes but using += — should be significantly faster.
TEST_F(StringRefTest, Concat_10k_AppendAssign)
{
    constexpr int N = 10'000;
    StringRef part("X");
    StringRef result;
    result.reserve(N);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        result += part;
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.len(), static_cast<size_t>(N));
    std::printf("  Concat 10k via +=:               %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// Copy-on-Write semantics under load
// ---------------------------------------------------------------------------

// 100k shallow copies (no mutation) — should be near-free if CoW shares data.
TEST_F(StringRefTest, CoW_100k_ShallowCopies)
{
    constexpr int N = 100'000;
    StringRef original("The quick brown fox jumps over the lazy dog");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef copy = original;
        do_not_optimize(copy);
    }
    double us = microseconds_since(t0);

    std::printf("  CoW 100k shallow copies:         %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 100k copy-then-mutate — each forces a real allocation (CoW break).
TEST_F(StringRefTest, CoW_100k_CopyThenMutate)
{
    constexpr int N = 100'000;
    StringRef original("The quick brown fox jumps over the lazy dog");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef copy = original;
        copy[0] = 'X'; // triggers CoW detach
        do_not_optimize(copy);
    }
    double us = microseconds_since(t0);

    std::printf("  CoW 100k copy+mutate (break):    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Ratio test: shallow copy should be substantially faster than copy+mutate.
TEST_F(StringRefTest, CoW_ShallowVsMutate_Ratio)
{
    constexpr int N = 50'000;
    StringRef original("The quick brown fox jumps over the lazy dog");

    // shallow
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef copy = original;
        do_not_optimize(copy);
    }
    double shallow_us = microseconds_since(t0);

    // mutating
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef copy = original;
        copy[0] = 'X';
        do_not_optimize(copy);
    }
    double mutate_us = microseconds_since(t0);

    std::printf("  CoW ratio: shallow=%.1f µs  mutate=%.1f µs  ratio=%.1fx\n",
        shallow_us, mutate_us, mutate_us / shallow_us);

    // Shallow copy must be at least 2x faster than copy+mutate.
    // If CoW is working, it should be much more than 2x.
    EXPECT_LT(shallow_us, mutate_us * 0.5)
        << "CoW shallow copy is not significantly faster than copy+mutate — "
           "CoW benefit may not be materializing";
}

// ---------------------------------------------------------------------------
// Hashing throughput
// ---------------------------------------------------------------------------

// Hash 1M times — exercises the hot path in hash maps.
TEST_F(StringRefTest, Hash_1M_Short)
{
    constexpr int N = 1'000'000;
    StringRefHash hasher;
    StringRef s("identifier_name");
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc ^= hasher(s);
    double us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Hash 1M (15-byte string):        %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

TEST_F(StringRefTest, Hash_1M_Long)
{
    constexpr int N = 1'000'000;
    StringRefHash hasher;
    std::string buf(256, 'x');
    StringRef s(buf.data());
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc ^= hasher(s);
    double us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Hash 1M (256-byte string):       %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Hash throughput should scale roughly linearly with length, not quadratically.
TEST_F(StringRefTest, Hash_ScalesWithLength)
{
    constexpr int N = 500'000;
    StringRefHash hasher;

    std::string short_buf(16, 'a');
    std::string long_buf(1024, 'a');
    StringRef s_short(short_buf.data());
    StringRef s_long(long_buf.data());
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc ^= hasher(s_short);
    double short_us = microseconds_since(t0);

    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc ^= hasher(s_long);
    double long_us = microseconds_since(t0);

    do_not_optimize(acc);
    double ratio = long_us / short_us;
    std::printf("  Hash scaling: 16B=%.1f µs  1024B=%.1f µs  ratio=%.1fx  "
                "(ideal=64x)\n",
        short_us, long_us, ratio);

    // Allow up to 128x — anything beyond suggests quadratic scanning.
    EXPECT_LT(ratio, 128.0)
        << "Hash appears to scale worse than O(n) with string length";
}

// ---------------------------------------------------------------------------
// Search / find
// ---------------------------------------------------------------------------

// find_pos over a 1 MB string — worst case (char not present).
TEST_F(StringRefTest, FindPos_1MB_Miss)
{
    constexpr size_t SZ = 1'000'000;
    std::string buf(SZ, 'A');
    StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = s.find_pos('Z'); // not present
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_FALSE(result.has_value());
    std::printf("  find_pos miss on 1MB string:     %.1f µs\n", us);
}

// find_pos hit at the very end.
TEST_F(StringRefTest, FindPos_1MB_HitAtEnd)
{
    constexpr size_t SZ = 1'000'000;
    std::string buf(SZ, 'A');
    buf.back() = 'Z';
    StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = s.find_pos('Z');
    double us = microseconds_since(t0);

    do_not_optimize(result);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), SZ - 1);
    std::printf("  find_pos hit-at-end on 1MB:      %.1f µs\n", us);
}

// ---------------------------------------------------------------------------
// substr / slice — zero-copy semantics under load
// ---------------------------------------------------------------------------

// 1M slices of a large string — should not allocate if slice is zero-copy.
TEST_F(StringRefTest, Slice_1M_NoAlloc)
{
    constexpr int N = 1'000'000;
    std::string buf(1000, 'X');
    StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef sl = s.slice(0, 500);
        do_not_optimize(sl);
    }
    double us = microseconds_since(t0);

    std::printf("  slice() 1M times (no mutation):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 1M substr calls — substr copies, so this exercises allocator throughput.
TEST_F(StringRefTest, Substr_1M_Copies)
{
    constexpr int N = 1'000'000;
    std::string buf(1000, 'X');
    StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef sub = s.substr(0, 500);
        do_not_optimize(sub);
    }
    double us = microseconds_since(t0);

    std::printf("  substr() 1M times (copies):      %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Slice should be significantly faster than substr (no alloc vs alloc).
TEST_F(StringRefTest, Slice_vs_Substr_Ratio)
{
    constexpr int N = 200'000;
    std::string buf(500, 'Y');
    StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef sl = s.slice(0, 250);
        do_not_optimize(sl);
    }
    double slice_us = microseconds_since(t0);

    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef sub = s.substr(0, 250);
        do_not_optimize(sub);
    }
    double substr_us = microseconds_since(t0);

    std::printf("  slice=%.1f µs  substr=%.1f µs  ratio=%.1fx\n",
        slice_us, substr_us, substr_us / slice_us);

    EXPECT_LT(slice_us, substr_us)
        << "slice() should be faster than substr() — slice is zero-copy";
}

// ---------------------------------------------------------------------------
// Equality comparison
// ---------------------------------------------------------------------------

// 2M equality checks on equal strings — hot path in interning / hash maps.
TEST_F(StringRefTest, Equality_2M_Equal)
{
    constexpr int N = 2'000'000;
    StringRef s1("some_variable_name");
    StringRef s2("some_variable_name");
    int hits = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        hits += (s1 == s2) ? 1 : 0;
    double us = microseconds_since(t0);

    do_not_optimize(hits);
    EXPECT_EQ(hits, N);
    std::printf("  Equality 2M (equal, 18B):        %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 2M equality checks on strings that differ in the last byte — worst case.
TEST_F(StringRefTest, Equality_2M_DifferLastByte)
{
    constexpr int N = 2'000'000;
    std::string a(64, 'A');
    a.back() = 'X';
    std::string b(64, 'A');
    b.back() = 'Y';
    StringRef s1(a.data());
    StringRef s2(b.data());
    int hits = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        hits += (s1 == s2) ? 1 : 0;
    double us = microseconds_since(t0);

    do_not_optimize(hits);
    EXPECT_EQ(hits, 0);
    std::printf("  Equality 2M (differ last, 64B):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// erase throughput
// ---------------------------------------------------------------------------

// Erase first char 100k times from the front — O(n) per erase → O(n²) total.
// Documents the cost so regressions are visible.
TEST_F(StringRefTest, Erase_100k_FromFront)
{
    constexpr int N = 100'000;
    StringRef s;
    s.reserve(N);
    for (int i = 0; i < N; ++i)
        s += char('A' + i % 26);

    auto t0 = std::chrono::high_resolution_clock::now();
    while (!s.empty())
        s.erase(0);
    double us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_TRUE(s.empty());
    std::printf("  Erase front 100k chars:          %.1f µs\n", us);
}

// ---------------------------------------------------------------------------
// Unicode / UTF-8 throughput
// ---------------------------------------------------------------------------

// Append 100k Arabic codepoints (2 bytes each in UTF-8).
TEST_F(StringRefTest, Append_100k_ArabicChunks)
{
    constexpr int N = 100'000;
    StringRef chunk("مرحبا"); // 10 UTF-8 bytes
    StringRef s;
    s.reserve(N * 10);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        s += chunk;
    double us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N * 10));
    std::printf("  Append 100k Arabic chunks:       %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// toDouble throughput
// ---------------------------------------------------------------------------

TEST_F(StringRefTest, ToDouble_1M_Integer)
{
    constexpr int N = 1'000'000;
    StringRef s("123456");
    double acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc += s.toDouble();
    double us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  toDouble() 1M integer parses:    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

TEST_F(StringRefTest, ToDouble_1M_Float)
{
    constexpr int N = 1'000'000;
    StringRef s("3.14159265");
    double acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i)
        acc += s.toDouble();
    double us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  toDouble() 1M float parses:      %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// Mixed workload — simulates a realistic interpreter inner loop:
// intern a name, look it up, compare, slice.
// ---------------------------------------------------------------------------
TEST_F(StringRefTest, Mixed_InterpreterInnerLoop)
{
    constexpr int N = 500'000;

    // Simulate identifier table with a small set of names.
    std::vector<StringRef> identifiers = {
        StringRef("counter"),
        StringRef("result"),
        StringRef("index"),
        StringRef("value"),
        StringRef("accumulator"),
    };

    StringRefHash hasher;
    size_t acc = 0;
    int matches = 0;
    StringRef target("result");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        StringRef const& id = identifiers[i % identifiers.size()];
        acc ^= hasher(id);                                       // hash lookup
        matches += (id == target);                               // equality check
        StringRef sl = id.slice(0, id.len() > 3 ? 3 : id.len()); // prefix slice
        do_not_optimize(sl);
    }
    double us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Mixed interpreter loop 500k:     %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
    std::printf("    (matches=%d, hash_acc=0x%zx)\n", matches, acc);
}
