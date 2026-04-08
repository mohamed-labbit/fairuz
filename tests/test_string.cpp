#include "../include/string.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

using namespace fairuz;

class Fa_StringRefTest : public ::testing::Test {
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(Fa_StringRefTest, SizeConstructor_Zero)
{
    Fa_StringRef s(static_cast<size_t>(0));
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
}

TEST_F(Fa_StringRefTest, SizeConstructor_SmallSize)
{
    Fa_StringRef s(10);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
    EXPECT_GE(s.cap(), 10);
}

TEST_F(Fa_StringRefTest, SizeConstructor_LargeSize)
{
    Fa_StringRef s(10000);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.len(), 0);
    EXPECT_GE(s.cap(), 10000);
}

TEST_F(Fa_StringRefTest, CopyConstructor_Empty)
{
    Fa_StringRef s1;
    Fa_StringRef s2(s1);
    EXPECT_TRUE(s2.empty());
    EXPECT_EQ(s2.len(), 0);
}

TEST_F(Fa_StringRefTest, CopyConstructorDetailed)
{
    Fa_StringRef s1("Hell");
    Fa_StringRef s2(s1);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s1.len(), s2.len());
    s1 += 'H';

    EXPECT_NE(s1, s2);
    EXPECT_EQ(s1, Fa_StringRef("HellH"));
    EXPECT_EQ(s2, Fa_StringRef("Hell"));

    EXPECT_NE(s1.data(), s2.data());
}

TEST_F(Fa_StringRefTest, CopyConstructorArabic)
{
    Fa_StringRef s1("مرحبا");
    Fa_StringRef s2(s1);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s2, Fa_StringRef("مرحبا"));
}

TEST_F(Fa_StringRefTest, CheckMemoryIndependence)
{
    Fa_StringRef s1("Test");
    Fa_StringRef s2(s1);

    s1[0] = 'X';

    char* ptr1 = s1.data();
    char* ptr2 = s2.data();

    EXPECT_NE(ptr1, ptr2) << "After CoW, should have different memory!";
    EXPECT_EQ(s1[0], 'X');
    EXPECT_EQ(s2[0], 'T');
    EXPECT_EQ(s1, "Xest");
    EXPECT_EQ(s2, "Test");
}

TEST_F(Fa_StringRefTest, CopyConstructor_NonEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2(s1);
    EXPECT_EQ(s2.len(), s1.len());
    EXPECT_EQ(s2, s1);
}

TEST_F(Fa_StringRefTest, CopyConstructor_Arabic)
{
    Fa_StringRef s1("مرحبا");
    Fa_StringRef s2(s1);
    EXPECT_EQ(s2, s1);
    EXPECT_EQ(s2, "مرحبا");
}

TEST_F(Fa_StringRefTest, MoveConstructor_NonEmpty)
{
    Fa_StringRef s1("Hello");
    char const* old_ptr = s1.data();
    size_t old_len = s1.len();

    Fa_StringRef s2(std::move(s1));

    EXPECT_EQ(s2.len(), old_len);
    EXPECT_EQ(s2.data(), old_ptr);
    EXPECT_TRUE(s1.empty());
    EXPECT_EQ(s1.data(), nullptr);
}

TEST_F(Fa_StringRefTest, CStyleConstructor_Null)
{
    char const* null_ptr = nullptr;
    Fa_StringRef s(null_ptr);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, CStyleConstructor_Empty)
{
    char empty[] = { char { 0 } };
    Fa_StringRef s(empty);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, CStyleConstructor_NonEmpty)
{
    char hello[] = { 'H', 'e', 'l', 'l', 'o', char { 0 } };
    Fa_StringRef s(hello);
    EXPECT_EQ(s.len(), 5);
    EXPECT_EQ(s[0], 'H');
    EXPECT_EQ(s[4], 'o');
}

TEST_F(Fa_StringRefTest, CopyAssignment_SelfAssignment)
{
    Fa_StringRef s("Test");
    s = s;
    EXPECT_EQ(s, "Test");
}

TEST_F(Fa_StringRefTest, CopyAssignment_SharedData)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2 = s1.slice(0, 2);

    s1 = s2;
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, CopyAssignment_EmptyToEmpty)
{
    Fa_StringRef s1, s2;
    s2 = s1;
    EXPECT_TRUE(s2.empty());
}

TEST_F(Fa_StringRefTest, CopyAssignment_NonEmptyToEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2;
    s2 = s1;
    EXPECT_EQ(s2, s1);
}

TEST_F(Fa_StringRefTest, CopyAssignment_EmptyToNonEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2;
    s1 = s2;
    EXPECT_TRUE(s1.empty());
}

TEST_F(Fa_StringRefTest, CopyAssignment_ReplaceContent)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("World");
    s1 = s2;
    EXPECT_EQ(s1, "World");
}

TEST_F(Fa_StringRefTest, MoveAssignment_SelfAssignment)
{
    Fa_StringRef s("Test");
    s = std::move(s);
    EXPECT_EQ(s, "Test");
}

TEST_F(Fa_StringRefTest, MoveAssignment_NonEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("World");
    char const* old_ptr = s2.data();

    s1 = std::move(s2);

    EXPECT_EQ(s1.data(), old_ptr);
    EXPECT_EQ(s1, "World");
    EXPECT_TRUE(s2.empty());
}

TEST_F(Fa_StringRefTest, Utf8Assignment_FromCString)
{
    Fa_StringRef s;
    s = "Hello";
    EXPECT_EQ(s, "Hello");
}

TEST_F(Fa_StringRefTest, Utf8Assignment_NullPtr)
{
    Fa_StringRef s("Hello");
    s = static_cast<char const*>(nullptr);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, Equality_BothEmpty)
{
    Fa_StringRef s1, s2;
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_SameContent)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("Hello");
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_DifferentContent)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("World");
    EXPECT_NE(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_DifferentLength)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("Hi");
    EXPECT_NE(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_SelfComparison)
{
    Fa_StringRef s("Test");
    EXPECT_EQ(s, s);
}

TEST_F(Fa_StringRefTest, Equality_ArabicText)
{
    Fa_StringRef s1("مرحبا بك");
    Fa_StringRef s2("مرحبا بك");
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_ChineseText)
{
    Fa_StringRef s1("你好世界");
    Fa_StringRef s2("你好世界");
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, Equality_Emoji)
{
    Fa_StringRef s1("Hello 😀🎉");
    Fa_StringRef s2("Hello 😀🎉");
    EXPECT_EQ(s1, s2);
}

TEST_F(Fa_StringRefTest, Expand_FromEmpty)
{
    Fa_StringRef s;
    s.expand(100);
    EXPECT_GE(s.cap(), 100);
    EXPECT_EQ(s.len(), 0);
}

TEST_F(Fa_StringRefTest, Expand_AlreadyLargeEnough)
{
    Fa_StringRef s(100);
    size_t old_cap = s.cap();
    s.expand(50);
    EXPECT_EQ(s.cap(), old_cap);
}

TEST_F(Fa_StringRefTest, Expand_GrowthFactor)
{
    Fa_StringRef s(10);
    s.expand(100);
    EXPECT_GE(s.cap(), 100);
}

TEST_F(Fa_StringRefTest, Reserve_IncreasesCapacity)
{
    Fa_StringRef s;
    s.reserve(1000);
    EXPECT_GE(s.cap(), 1000);
    EXPECT_EQ(s.len(), 0);
}

TEST_F(Fa_StringRefTest, Reserve_PreservesContent)
{
    Fa_StringRef s("Hello");
    s.reserve(1000);
    EXPECT_EQ(s, "Hello");
    EXPECT_GE(s.cap(), 1000);
}

TEST_F(Fa_StringRefTest, Clear_EmptiesString)
{
    Fa_StringRef s("Hello");
    s.clear();
    EXPECT_EQ(s.len(), 0);
    EXPECT_TRUE(s.empty());
    EXPECT_GT(s.cap(), 0);
}

TEST_F(Fa_StringRefTest, Clear_MultipleTime)
{
    Fa_StringRef s("Hello");
    s.clear();
    s.clear();
    s.clear();
    EXPECT_EQ(s.len(), 0);
}

TEST_F(Fa_StringRefTest, Resize_IncreasesCapacity)
{
    Fa_StringRef s;
    s.resize(500);
    EXPECT_GE(s.cap(), 500);
}

TEST_F(Fa_StringRefTest, AppendChar_ToEmpty)
{
    Fa_StringRef s;
    s += 'A';
    EXPECT_EQ(s.len(), 1);
    EXPECT_EQ(s[0], 'A');
}

TEST_F(Fa_StringRefTest, AppendChar_Multiple)
{
    Fa_StringRef s;
    s += 'H';
    s += 'i';
    EXPECT_EQ(s.len(), 2);
    EXPECT_EQ(s, "Hi");
}

TEST_F(Fa_StringRefTest, AppendChar_UnicodeChar)
{
    Fa_StringRef s;
    s += char(0x0645);
    EXPECT_GE(s.len(), 1);
}

TEST_F(Fa_StringRefTest, AppendChar_TriggerExpansion)
{
    Fa_StringRef s(2);
    for (int i = 0; i < 100; i += 1)
        s += 'A';
    EXPECT_EQ(s.len(), 100);
}

TEST_F(Fa_StringRefTest, AppendString_ToEmpty)
{
    Fa_StringRef s1;
    Fa_StringRef s2("Hello");
    s1 += s2;
    EXPECT_EQ(s1, "Hello");
}

TEST_F(Fa_StringRefTest, AppendString_EmptyToNonEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2;
    s1 += s2;
    EXPECT_EQ(s1, "Hello");
}

TEST_F(Fa_StringRefTest, AppendString_NonEmptyToNonEmpty)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2(" World");
    s1 += s2;
    EXPECT_EQ(s1, "Hello World");
}

TEST_F(Fa_StringRefTest, AppendString_Arabic)
{
    Fa_StringRef s1("مرحبا");
    Fa_StringRef s2(" بك");
    s1 += s2;
    EXPECT_EQ(s1, "مرحبا بك");
}

TEST_F(Fa_StringRefTest, AppendString_TriggerReallocation)
{
    Fa_StringRef s1(5);
    Fa_StringRef s2("This is a very long string that will trigger reallocation");
    s1 += s2;
    EXPECT_EQ(s1, s2);
}

// concatenation

TEST_F(Fa_StringRefTest, Concat_StringPlusString)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2(" World");
    Fa_StringRef s3 = s1 + s2;
    EXPECT_EQ(s3, "Hello World");
}

TEST_F(Fa_StringRefTest, Concat_EmptyPlusEmpty)
{
    Fa_StringRef s1, s2;
    Fa_StringRef s3 = s1 + s2;
    EXPECT_TRUE(s3.empty());
}

TEST_F(Fa_StringRefTest, Concat_EmptyPlusNonEmpty)
{
    Fa_StringRef s1;
    Fa_StringRef s2("Hello");
    Fa_StringRef s3 = s1 + s2;
    EXPECT_EQ(s3, "Hello");
}

TEST_F(Fa_StringRefTest, Concat_StringPlusCString)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2 = s1 + " World";
    EXPECT_EQ(s2, "Hello World");
}

TEST_F(Fa_StringRefTest, Concat_CStringPlusString)
{
    Fa_StringRef s1(" World");
    Fa_StringRef s2 = "Hello" + s1;
    EXPECT_EQ(s2, "Hello World");
}

TEST_F(Fa_StringRefTest, Concat_StringPlusChar)
{
    Fa_StringRef s1("Hell");
    Fa_StringRef s2 = s1 + char('o');
    EXPECT_EQ(s2, "Hello");
}

TEST_F(Fa_StringRefTest, Concat_CharPlusString)
{
    Fa_StringRef s1("ello");
    Fa_StringRef s2 = char('H') + s1;
    EXPECT_EQ(s2, "Hello");
}

TEST_F(Fa_StringRefTest, Concat_Chain)
{
    Fa_StringRef s1("A");
    Fa_StringRef s2("B");
    Fa_StringRef s3("C");
    Fa_StringRef result = s1 + s2 + s3;
    EXPECT_EQ(result, "ABC");
}

TEST_F(Fa_StringRefTest, Concat_NullCString)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2 = s1 + static_cast<char const*>(nullptr);
    EXPECT_EQ(s2, "Hello");
}

TEST_F(Fa_StringRefTest, IndexOperator_ValidAccess)
{
    Fa_StringRef s("Hello");
    EXPECT_EQ(s[0], 'H');
    EXPECT_EQ(s[4], 'o');
}

TEST_F(Fa_StringRefTest, IndexOperator_Mutable)
{
    Fa_StringRef s("Hello");
    s[0] = 'J';
    EXPECT_EQ(s[0], 'J');
}

TEST_F(Fa_StringRefTest, At_ValidAccess)
{
    Fa_StringRef s("Hello");
    EXPECT_EQ(s.at(0), 'H');
    EXPECT_EQ(s.at(4), 'o');
}

TEST_F(Fa_StringRefTest, At_MutableAccess)
{
    Fa_StringRef s("Hello");
    s.at(0) = 'J';
    EXPECT_EQ(s.at(0), 'J');
}

TEST_F(Fa_StringRefTest, Erase_FirstChar)
{
    Fa_StringRef s("Hello");
    s.erase(0);
    EXPECT_EQ(s, "ello");
}

TEST_F(Fa_StringRefTest, Erase_LastChar)
{
    Fa_StringRef s("Hello");
    s.erase(4);
    EXPECT_EQ(s, "Hell");
}

TEST_F(Fa_StringRefTest, Erase_MiddleChar)
{
    Fa_StringRef s("Hello");
    s.erase(2);
    EXPECT_EQ(s, "Helo");
}

TEST_F(Fa_StringRefTest, Erase_OutOfBounds)
{
    Fa_StringRef s("Hello");
    s.erase(100);
    EXPECT_EQ(s, "Hello");
}

TEST_F(Fa_StringRefTest, Erase_EmptyString)
{
    Fa_StringRef s;
    s.erase(0);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, Erase_AllChars)
{
    Fa_StringRef s("Hi");
    s.erase(0);
    s.erase(0);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, Find_CharExists)
{
    Fa_StringRef s("Hello");
    EXPECT_TRUE(s.find('H'));
    EXPECT_TRUE(s.find('o'));
}

TEST_F(Fa_StringRefTest, Find_CharNotExists)
{
    Fa_StringRef s("Hello");
    EXPECT_FALSE(s.find('X'));
}

TEST_F(Fa_StringRefTest, Find_EmptyString)
{
    Fa_StringRef s;
    EXPECT_FALSE(s.find('A'));
}

TEST_F(Fa_StringRefTest, FindPos_CharExists)
{
    Fa_StringRef s("Hello");
    auto pos = s.find_pos('e');
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 1);
}

TEST_F(Fa_StringRefTest, FindPos_CharNotExists)
{
    Fa_StringRef s("Hello");
    auto pos = s.find_pos('X');
    EXPECT_FALSE(pos.has_value());
}

TEST_F(Fa_StringRefTest, FindPos_FirstOccurrence)
{
    Fa_StringRef s("Hello");
    auto pos = s.find_pos('l');
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 2);
}

TEST_F(Fa_StringRefTest, Truncate_ToShorter)
{
    Fa_StringRef s("Hello");
    s.truncate(3);
    EXPECT_EQ(s.len(), 3);
    EXPECT_EQ(s, "Hel");
}

TEST_F(Fa_StringRefTest, Truncate_ToZero)
{
    Fa_StringRef s("Hello");
    s.truncate(0);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, Truncate_ToLargerSize)
{
    Fa_StringRef s("Hello");
    s.truncate(100);
    EXPECT_EQ(s, "Hello");
}

TEST_F(Fa_StringRefTest, Truncate_EmptyString)
{
    Fa_StringRef s;
    s.truncate(5);
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, Substr_FullString)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sub = s.substr(0, 5);
    EXPECT_EQ(sub, "Hello");
}

TEST_F(Fa_StringRefTest, Substr_MiddlePortion)
{
    Fa_StringRef s("Hello World");
    Fa_StringRef sub = s.substr(6, 11);
    EXPECT_EQ(sub, "World");
}

TEST_F(Fa_StringRefTest, Substr_FirstChar)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sub = s.substr(0, 1);
    EXPECT_EQ(sub.len(), 1);
    EXPECT_EQ(sub[0], 'H');
}

TEST_F(Fa_StringRefTest, Substr_LastChar)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sub = s.substr(4, 5);
    EXPECT_EQ(sub.len(), 1);
    EXPECT_EQ(sub[0], 'o');
}

TEST_F(Fa_StringRefTest, Substr_DefaultEnd)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sub = s.substr(2);
    EXPECT_EQ(sub, "llo");
}

TEST_F(Fa_StringRefTest, Substr_EmptyString)
{
    Fa_StringRef s;
    Fa_StringRef sub = s.substr(0, 1);
    EXPECT_TRUE(sub.empty());
}

TEST_F(Fa_StringRefTest, Substr_StartOutOfBounds)
{
    Fa_StringRef s("Hello");
    EXPECT_EQ(s.substr(100, 101), Fa_StringRef { });
}

TEST_F(Fa_StringRefTest, Substr_EndLargerThanLength)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sub = s.substr(0, 101);
    EXPECT_EQ(sub, "Hello");
}

TEST_F(Fa_StringRefTest, Substr_EndBeforeStart)
{
    Fa_StringRef s("Hello");
    EXPECT_EQ(s.substr(4, 3), Fa_StringRef { });
}

TEST_F(Fa_StringRefTest, Substr_ArabicText)
{
    Fa_StringRef s("مرحبا بك في العالم");
    Fa_StringRef sub = s.substr(0, 10);
    EXPECT_EQ(sub, "مرحبا");
}

// ADDED: slice tests
TEST_F(Fa_StringRefTest, Slice_FullString)
{
    Fa_StringRef s("Hello");
    Fa_StringRef sliced = s.slice(0, 5);
    EXPECT_EQ(sliced, "Hello");
}

TEST_F(Fa_StringRefTest, Slice_ChainedSlices)
{
    Fa_StringRef s("Hello World");
    Fa_StringRef s1 = s.slice(0, 5);
    Fa_StringRef s2 = s1.slice(1, 4);
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "ell");
}

TEST_F(Fa_StringRefTest, Slice_SharesMemory)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2 = s1.slice(0, 3);

    EXPECT_EQ(s1.get(), s2.get());

    s2[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Hello");
    EXPECT_EQ(s2, "Jel");
}

TEST_F(Fa_StringRefTest, FromUtf8_EmptyString)
{
    Fa_StringRef s("");
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, FromUtf8_AsciiString)
{
    Fa_StringRef s("Hello World");
    EXPECT_EQ(s, "Hello World");
}

TEST_F(Fa_StringRefTest, FromUtf8_ArabicString)
{
    Fa_StringRef s("مرحبا");
    EXPECT_EQ(s, "مرحبا");
}

TEST_F(Fa_StringRefTest, FromUtf8_ChineseString)
{
    Fa_StringRef s("你好世界");
    EXPECT_EQ(s, "你好世界");
}

TEST_F(Fa_StringRefTest, FromUtf8_JapaneseString)
{
    Fa_StringRef s("こんにちは");
    EXPECT_EQ(s, "こんにちは");
}

TEST_F(Fa_StringRefTest, FromUtf8_RussianString)
{
    Fa_StringRef s("Привет мир");
    EXPECT_EQ(s, "Привет мир");
}

TEST_F(Fa_StringRefTest, FromUtf8_EmojiString)
{
    Fa_StringRef s("Hello 😀 World 🎉");
    EXPECT_EQ(s, "Hello 😀 World 🎉");
}

TEST_F(Fa_StringRefTest, FromUtf8_MixedScript)
{
    Fa_StringRef s("Hello مرحبا 你好 🌍");
    EXPECT_EQ(s, "Hello مرحبا 你好 🌍");
}

TEST_F(Fa_StringRefTest, FromUtf8_NullPtr)
{
    Fa_StringRef s(static_cast<char const*>(nullptr));
    EXPECT_TRUE(s.empty());
}

TEST_F(Fa_StringRefTest, ToUtf8_EmptyString)
{
    Fa_StringRef s;
    EXPECT_EQ(s, "");
}

TEST_F(Fa_StringRefTest, ToUtf8_RoundTrip)
{
    std::string original = "Hello World مرحبا 你好 😀";
    Fa_StringRef s(original.data());
    std::string result = s.data();
    EXPECT_EQ(result, original);
}

TEST_F(Fa_StringRefTest, ToDouble_Integer)
{
    Fa_StringRef s("42");
    EXPECT_DOUBLE_EQ(s.to_double(), 42.0);
}

TEST_F(Fa_StringRefTest, ToDouble_Float)
{
    Fa_StringRef s("3.14159");
    EXPECT_DOUBLE_EQ(s.to_double(), 3.14159);
}

TEST_F(Fa_StringRefTest, ToDouble_Negative)
{
    Fa_StringRef s("-123.456");
    EXPECT_DOUBLE_EQ(s.to_double(), -123.456);
}

TEST_F(Fa_StringRefTest, ToDouble_Scientific)
{
    Fa_StringRef s("1.5e10");
    EXPECT_DOUBLE_EQ(s.to_double(), 1.5e10);
}

TEST_F(Fa_StringRefTest, ToDouble_WithPosition)
{
    Fa_StringRef s("123.456abc");
    size_t pos = 0;
    f64 result = s.to_double(&pos);
    EXPECT_DOUBLE_EQ(result, 123.456);
    EXPECT_GT(pos, 0);
}

TEST_F(Fa_StringRefTest, Hash_EmptyString)
{
    Fa_StringRefHash hasher;
    Fa_StringRef s;
    EXPECT_EQ(hasher(s), 0);
}

TEST_F(Fa_StringRefTest, Hash_NonEmpty)
{
    Fa_StringRefHash hasher;
    Fa_StringRef s("Hello");
    size_t m_hash = hasher(s);
    EXPECT_NE(m_hash, 0);
}

TEST_F(Fa_StringRefTest, Hash_SameContent)
{
    Fa_StringRefHash hasher;
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("Hello");
    EXPECT_EQ(hasher(s1), hasher(s2));
}

TEST_F(Fa_StringRefTest, Hash_DifferentContent)
{
    Fa_StringRefHash hasher;
    Fa_StringRef s1("Hello");
    Fa_StringRef s2("World");
    EXPECT_NE(hasher(s1), hasher(s2));
}

TEST_F(Fa_StringRefTest, StdHash_Works)
{
    std::hash<Fa_StringRef> hasher;
    Fa_StringRef s("Test");
    size_t m_hash = hasher(s);
    EXPECT_NE(m_hash, 0);
}

TEST_F(Fa_StringRefTest, Stress_ManyAppends)
{
    Fa_StringRef s;
    for (int i = 0; i < 10000; i += 1)
        s += char('A' + (i % 26));
    EXPECT_EQ(s.len(), 10000);
}

TEST_F(Fa_StringRefTest, Stress_ManyErases)
{
    Fa_StringRef s;
    for (int i = 0; i < 1000; i += 1)
        s += char('A');
    for (int i = 0; i < 500; i += 1)
        s.erase(0);
    EXPECT_EQ(s.len(), 500);
}

TEST_F(Fa_StringRefTest, Stress_CopyAndModify)
{
    Fa_StringRef original("Original");
    std::vector<Fa_StringRef> copies;

    for (int i = 0; i < 100; i += 1) {
        copies.push_back(original);
        copies.back() += char('0' + i % 10);
    }

    EXPECT_EQ(original, "Original");
    for (size_t i = 0; i < copies.size(); i += 1)
        EXPECT_NE(copies[i], original);
}

TEST_F(Fa_StringRefTest, Stress_LargeString)
{
    std::string large(100000, 'A');
    Fa_StringRef s(large.data());
    EXPECT_EQ(s.len(), 100000);
}

TEST_F(Fa_StringRefTest, Stress_UnicodeAppends)
{
    Fa_StringRef s;
    for (int i = 0; i < 1000; i += 1)
        s = s + "مرحبا";

    EXPECT_EQ(s.len(), 10000);
}

TEST_F(Fa_StringRefTest, Stress_ManySubstrings)
{
    Fa_StringRef s("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    std::vector<Fa_StringRef> subs;

    for (size_t i = 0; i < s.len(); i += 1) {
        for (size_t j = i; j < s.len(); j += 1)
            subs.push_back(s.substr(i, j));
    }

    EXPECT_GT(subs.size(), 100);
}

TEST_F(Fa_StringRefTest, Stress_RandomOperations)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<> op_dist(0, 4);
    std::uniform_int_distribution<> char_dist('A', 'Z');

    Fa_StringRef s;

    for (int i = 0; i < 1000; i += 1) {
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
            Fa_StringRef copy = s;
            s = copy;
        } break;
        }
    }

    EXPECT_TRUE(true);
}

TEST_F(Fa_StringRefTest, EdgeCase_NullTerminatorInMiddle)
{
    Fa_StringRef s("Hello");
    s[2] = char { 0 };
    EXPECT_LE(s.len(), 5);
}

TEST_F(Fa_StringRefTest, EdgeCase_MaxSizeString)
{
    size_t const large_size = 1000000;
    Fa_StringRef s(large_size);
    for (size_t i = 0; i < 100; i += 1)
        s += 'A';
    EXPECT_EQ(s.len(), 100);
}

TEST_F(Fa_StringRefTest, EdgeCase_EmptyOperations)
{
    Fa_StringRef s;
    s += Fa_StringRef();
    s.clear();
    s.erase(0);
    s.truncate(0);
    auto sub = s.substr(0);
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(sub.empty());
}

TEST_F(Fa_StringRefTest, EdgeCase_SurrogatesPairs)
{
    Fa_StringRef s("😀😃😄😁");
    EXPECT_GT(s.len(), 4);
}

TEST_F(Fa_StringRefTest, EdgeCase_AllZeros)
{
    Fa_StringRef s(10);
    for (int i = 0; i < 10; i += 1)
        s += char { 0 };
    EXPECT_EQ(s.len(), 10);
}

TEST_F(Fa_StringRefTest, EdgeCase_HighUnicodeValues)
{
    Fa_StringRef s("𝕳𝖊𝖑𝖑𝖔");
    EXPECT_GT(s.len(), 5);
}

TEST_F(Fa_StringRefTest, NoLeak_MultipleConstructDestruct)
{
    for (int i = 0; i < 1000; i += 1)
        Fa_StringRef s("Test String");
    EXPECT_TRUE(true);
}

TEST_F(Fa_StringRefTest, NoLeak_CopyAssignmentLoop)
{
    Fa_StringRef original("Original");
    for (int i = 0; i < 100; i += 1) {
        Fa_StringRef copy;
        copy = original;
    }
    EXPECT_TRUE(true);
}

TEST_F(Fa_StringRefTest, NoLeak_MoveAssignmentLoop)
{
    for (int i = 0; i < 100; i += 1) {
        Fa_StringRef s1("Test");
        Fa_StringRef s2 = std::move(s1);
    }
    EXPECT_TRUE(true);
}

TEST_F(Fa_StringRefTest, Performance_AppendChars)
{
    auto start = std::chrono::high_resolution_clock::now();

    Fa_StringRef s;
    s.reserve(10000);
    for (int i = 0; i < 10000; i += 1)
        s += char('A');

    auto m_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - start);

    EXPECT_EQ(s.len(), 10000);
    std::cout << "Append 10000 chars took: " << duration.count() << "ms\n";
}

TEST_F(Fa_StringRefTest, Performance_Concatenation)
{
    auto start = std::chrono::high_resolution_clock::now();

    Fa_StringRef result;
    Fa_StringRef part("Part");

    for (int i = 0; i < 1000; i += 1)
        result = result + part;

    auto m_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - start);

    EXPECT_EQ(result.len(), 4000);
    std::cout << "1000 concatenations took: " << duration.count() << "ms\n";
}

TEST_F(Fa_StringRefTest, Performance_Utf8Conversion)
{
    std::string utf8(10000, 'A');

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i += 1) {
        Fa_StringRef s(utf8.data());
        std::string back = s.data();
    }

    auto m_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - start);

    std::cout << "100 UTF-8 round-trips (10k chars) took: " << duration.count() << "ms\n";
}

TEST_F(Fa_StringRefTest, TestTrim)
{
    Fa_StringRef s1 = "abc   ";
    Fa_StringRef s2 = "   abc";
    Fa_StringRef s3 = "   abc    ";

    s1.trim_whitespace();
    s2.trim_whitespace();
    s3.trim_whitespace();

    EXPECT_EQ(s1, Fa_StringRef("abc"));
    EXPECT_EQ(s2, Fa_StringRef("abc"));
    EXPECT_EQ(s3, Fa_StringRef("abc"));
}

TEST_F(Fa_StringRefTest, CoW_BasicSharing)
{
    Fa_StringRef s1("Hello");
    Fa_StringRef s2 = s1;

    EXPECT_EQ(s1.get(), s2.get());

    s1[0] = 'J';
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(s1, "Jello");
    EXPECT_EQ(s2, "Hello");
}

TEST_F(Fa_StringRefTest, CoW_NonConstData)
{
    Fa_StringRef s1("Test");
    Fa_StringRef s2 = s1;

    char* ptr1 = s1.data();
    char const* ptr2 = s2.data();

    EXPECT_NE(ptr1, ptr2);
}

static f64 microseconds_since(std::chrono::high_resolution_clock::time_point t0)
{
    using namespace std::chrono;
    return static_cast<f64>(
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
TEST_F(Fa_StringRefTest, Append_1M_Chars_PreReserved)
{
    constexpr int N = 1'000'000;

    Fa_StringRef s;
    s.reserve(N);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        s += 'A';
    f64 us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N));
    std::printf("  Append 1M chars (pre-reserved):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Append with no pre-reservation — measures reallocation overhead.
TEST_F(Fa_StringRefTest, Append_1M_Chars_NoReserve)
{
    constexpr int N = 1'000'000;

    Fa_StringRef s;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        s += char('A' + i % 26);
    f64 us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N));
    std::printf("  Append 1M chars (no reserve):    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Append short string chunks — 250k * 4 bytes = 1 MB total.
TEST_F(Fa_StringRefTest, Append_250k_ShortStrings)
{
    constexpr int N = 250'000;
    Fa_StringRef chunk("abcd");

    Fa_StringRef s;
    s.reserve(N * 4);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        s += chunk;
    f64 us = microseconds_since(t0);

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
TEST_F(Fa_StringRefTest, Concat_10k_Growing)
{
    constexpr int N = 10'000;
    Fa_StringRef part("X");
    Fa_StringRef result;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        result = result + part;
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.len(), static_cast<size_t>(N));
    std::printf("  Concat 10k growing (operator+):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Same total bytes but using += — should be significantly faster.
TEST_F(Fa_StringRefTest, Concat_10k_AppendAssign)
{
    constexpr int N = 10'000;
    Fa_StringRef part("X");
    Fa_StringRef result;
    result.reserve(N);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        result += part;
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.len(), static_cast<size_t>(N));
    std::printf("  Concat 10k via +=:               %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// Copy-on-Write semantics under load
// ---------------------------------------------------------------------------

// 100k shallow copies (no mutation) — should be near-free if CoW shares data.
TEST_F(Fa_StringRefTest, CoW_100k_ShallowCopies)
{
    constexpr int N = 100'000;
    Fa_StringRef original("The quick brown fox jumps over the lazy dog");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef copy = original;
        do_not_optimize(copy);
    }
    f64 us = microseconds_since(t0);

    std::printf("  CoW 100k shallow copies:         %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 100k copy-then-mutate — each forces a real allocation (CoW break).
TEST_F(Fa_StringRefTest, CoW_100k_CopyThenMutate)
{
    constexpr int N = 100'000;
    Fa_StringRef original("The quick brown fox jumps over the lazy dog");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef copy = original;
        copy[0] = 'X'; // triggers CoW detach
        do_not_optimize(copy);
    }
    f64 us = microseconds_since(t0);

    std::printf("  CoW 100k copy+mutate (break):    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Ratio test: shallow copy should be substantially faster than copy+mutate.
TEST_F(Fa_StringRefTest, CoW_ShallowVsMutate_Ratio)
{
    constexpr int N = 50'000;
    Fa_StringRef original("The quick brown fox jumps over the lazy dog");

    // shallow
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef copy = original;
        do_not_optimize(copy);
    }
    f64 shallow_us = microseconds_since(t0);

    // mutating
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef copy = original;
        copy[0] = 'X';
        do_not_optimize(copy);
    }
    f64 mutate_us = microseconds_since(t0);

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
TEST_F(Fa_StringRefTest, Hash_1M_Short)
{
    constexpr int N = 1'000'000;
    Fa_StringRefHash hasher;
    Fa_StringRef s("identifier_name");
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc ^= hasher(s);
    f64 us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Hash 1M (15-byte string):        %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

TEST_F(Fa_StringRefTest, Hash_1M_Long)
{
    constexpr int N = 1'000'000;
    Fa_StringRefHash hasher;
    std::string buf(256, 'x');
    Fa_StringRef s(buf.data());
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc ^= hasher(s);
    f64 us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Hash 1M (256-byte string):       %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Hash throughput should scale roughly linearly with length, not quadratically.
TEST_F(Fa_StringRefTest, Hash_ScalesWithLength)
{
    constexpr int N = 500'000;
    Fa_StringRefHash hasher;

    std::string short_buf(16, 'a');
    std::string long_buf(1024, 'a');
    Fa_StringRef s_short(short_buf.data());
    Fa_StringRef s_long(long_buf.data());
    size_t acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc ^= hasher(s_short);
    f64 short_us = microseconds_since(t0);

    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc ^= hasher(s_long);
    f64 long_us = microseconds_since(t0);

    do_not_optimize(acc);
    f64 ratio = long_us / short_us;
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
TEST_F(Fa_StringRefTest, FindPos_1MB_Miss)
{
    constexpr size_t SZ = 1'000'000;
    std::string buf(SZ, 'A');
    Fa_StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = s.find_pos('Z'); // not present
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_FALSE(result.has_value());
    std::printf("  find_pos miss on 1MB string:     %.1f µs\n", us);
}

// find_pos hit at the very end.
TEST_F(Fa_StringRefTest, FindPos_1MB_HitAtEnd)
{
    constexpr size_t SZ = 1'000'000;
    std::string buf(SZ, 'A');
    buf.back() = 'Z';
    Fa_StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = s.find_pos('Z');
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), SZ - 1);
    std::printf("  find_pos hit-at-end on 1MB:      %.1f µs\n", us);
}

// ---------------------------------------------------------------------------
// substr / slice — zero-copy semantics under load
// ---------------------------------------------------------------------------

// 1M slices of a large string — should not allocate if slice is zero-copy.
TEST_F(Fa_StringRefTest, Slice_1M_NoAlloc)
{
    constexpr int N = 1'000'000;
    std::string buf(1000, 'X');
    Fa_StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef sl = s.slice(0, 500);
        do_not_optimize(sl);
    }
    f64 us = microseconds_since(t0);

    std::printf("  slice() 1M times (no mutation):  %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 1M substr calls — substr copies, so this exercises allocator throughput.
TEST_F(Fa_StringRefTest, Substr_1M_Copies)
{
    constexpr int N = 1'000'000;
    std::string buf(1000, 'X');
    Fa_StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef sub = s.substr(0, 500);
        do_not_optimize(sub);
    }
    f64 us = microseconds_since(t0);

    std::printf("  substr() 1M times (copies):      %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// Slice should be significantly faster than substr (no alloc vs alloc).
TEST_F(Fa_StringRefTest, Slice_vs_Substr_Ratio)
{
    constexpr int N = 200'000;
    std::string buf(500, 'Y');
    Fa_StringRef s(buf.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef sl = s.slice(0, 250);
        do_not_optimize(sl);
    }
    f64 slice_us = microseconds_since(t0);

    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef sub = s.substr(0, 250);
        do_not_optimize(sub);
    }
    f64 substr_us = microseconds_since(t0);

    std::printf("  slice=%.1f µs  substr=%.1f µs  ratio=%.1fx\n",
        slice_us, substr_us, substr_us / slice_us);

    EXPECT_LT(slice_us, substr_us)
        << "slice() should be faster than substr() — slice is zero-copy";
}

// ---------------------------------------------------------------------------
// Equality comparison
// ---------------------------------------------------------------------------

// 2M equality checks on equal strings — hot path in interning / hash maps.
TEST_F(Fa_StringRefTest, Equality_2M_Equal)
{
    constexpr int N = 2'000'000;
    Fa_StringRef s1("some_variable_name");
    Fa_StringRef s2("some_variable_name");
    int hits = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        hits += (s1 == s2) ? 1 : 0;
    f64 us = microseconds_since(t0);

    do_not_optimize(hits);
    EXPECT_EQ(hits, N);
    std::printf("  Equality 2M (equal, 18B):        %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// 2M equality checks on strings that differ in the last byte — worst case.
TEST_F(Fa_StringRefTest, Equality_2M_DifferLastByte)
{
    constexpr int N = 2'000'000;
    std::string a(64, 'A');
    a.back() = 'X';
    std::string b(64, 'A');
    b.back() = 'Y';
    Fa_StringRef s1(a.data());
    Fa_StringRef s2(b.data());
    int hits = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        hits += (s1 == s2) ? 1 : 0;
    f64 us = microseconds_since(t0);

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
TEST_F(Fa_StringRefTest, Erase_100k_FromFront)
{
    constexpr int N = 100'000;
    Fa_StringRef s;
    s.reserve(N);
    for (int i = 0; i < N; i += 1)
        s += char('A' + i % 26);

    auto t0 = std::chrono::high_resolution_clock::now();
    while (!s.empty())
        s.erase(0);
    f64 us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_TRUE(s.empty());
    std::printf("  Erase front 100k chars:          %.1f µs\n", us);
}

// ---------------------------------------------------------------------------
// Unicode / UTF-8 throughput
// ---------------------------------------------------------------------------

// Append 100k Arabic codepoints (2 bytes each in UTF-8).
TEST_F(Fa_StringRefTest, Append_100k_ArabicChunks)
{
    constexpr int N = 100'000;
    Fa_StringRef chunk("مرحبا"); // 10 UTF-8 bytes
    Fa_StringRef s;
    s.reserve(N * 10);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        s += chunk;
    f64 us = microseconds_since(t0);

    do_not_optimize(s);
    EXPECT_EQ(s.len(), static_cast<size_t>(N * 10));
    std::printf("  Append 100k Arabic chunks:       %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// toDouble throughput
// ---------------------------------------------------------------------------

TEST_F(Fa_StringRefTest, ToDouble_1M_Integer)
{
    constexpr int N = 1'000'000;
    Fa_StringRef s("123456");
    f64 acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc += s.to_double();
    f64 us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  toDouble() 1M integer parses:    %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

TEST_F(Fa_StringRefTest, ToDouble_1M_Float)
{
    constexpr int N = 1'000'000;
    Fa_StringRef s("3.14159265");
    f64 acc = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1)
        acc += s.to_double();
    f64 us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  toDouble() 1M float parses:      %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
}

// ---------------------------------------------------------------------------
// Mixed workload — simulates a realistic interpreter inner loop:
// intern a name, look it up, compare, slice.
// ---------------------------------------------------------------------------
TEST_F(Fa_StringRefTest, Mixed_InterpreterInnerLoop)
{
    constexpr int N = 500'000;

    // Simulate identifier table with a small set of names.
    std::vector<Fa_StringRef> identifiers = {
        Fa_StringRef("counter"),
        Fa_StringRef("result"),
        Fa_StringRef("index"),
        Fa_StringRef("value"),
        Fa_StringRef("accumulator"),
    };

    Fa_StringRefHash hasher;
    size_t acc = 0;
    int matches = 0;
    Fa_StringRef target("result");

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i += 1) {
        Fa_StringRef const& id = identifiers[i % identifiers.size()];
        acc ^= hasher(id);                                          // hash lookup
        matches += (id == target);                                  // equality check
        Fa_StringRef sl = id.slice(0, id.len() > 3 ? 3 : id.len()); // prefix slice
        do_not_optimize(sl);
    }
    f64 us = microseconds_since(t0);

    do_not_optimize(acc);
    std::printf("  Mixed interpreter loop 500k:     %.1f µs  (%.1f ns/op)\n",
        us, us * 1000.0 / N);
    std::printf("    (matches=%d, hash_acc=0x%zx)\n", matches, acc);
}
