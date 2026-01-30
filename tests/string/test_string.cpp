// stringref_test.cpp - Comprehensive brutal test suite for StringRef
#include "../../include/types.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

using namespace mylang;

// constructor

class StringRefTest: public ::testing::Test
{
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

TEST_F(StringRefTest, DefaultConstructor)
{
  StringRef s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.len(), 0);
  EXPECT_EQ(s.cap(), 0);
  EXPECT_EQ(s.data(), nullptr);
}

TEST_F(StringRefTest, SizeConstructor_Zero)
{
  StringRef s(static_cast<SizeType>(0));
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.len(), 0);
}

TEST_F(StringRefTest, SizeConstructor_SmallSize)
{
  StringRef s(10);
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.len(), 0);
  EXPECT_GE(s.cap(), 11);  // Should have space for 10 chars + null
}

TEST_F(StringRefTest, SizeConstructor_LargeSize)
{
  StringRef s(10000);
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.len(), 0);
  EXPECT_GE(s.cap(), 10001);
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
  // Create original
  StringRef s1 = StringRef::fromUtf8("Hell");
  std::cout << "s1 initial: " << s1.toUtf8() << " (len=" << s1.len() << ", cap=" << s1.cap() << ")\n";

  // Copy it
  StringRef s2(s1);
  std::cout << "s2 after copy: " << s2.toUtf8() << " (len=" << s2.len() << ", cap=" << s2.cap() << ")\n";

  // Verify they're equal
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(s1.len(), s2.len());
  EXPECT_NE(s1.data(), s2.data());  // Different memory

  // Now modify s1
  s1 += 'H';
  std::cout << "s1 after append: " << s1.toUtf8() << " (len=" << s1.len() << ")\n";
  std::cout << "s2 after s1 append: " << s2.toUtf8() << " (len=" << s2.len() << ")\n";

  // They should be different
  EXPECT_NE(s1, s2);
  EXPECT_EQ(s1.toUtf8(), "HellH");
  EXPECT_EQ(s2.toUtf8(), "Hell");
}

TEST(DiagnosticTest, CopyConstructorArabic)
{
  StringRef s1 = StringRef::fromUtf8("مرحبا");
  std::cout << "s1: " << s1.toUtf8() << " (len=" << s1.len() << ")\n";

  StringRef s2(s1);
  std::cout << "s2: " << s2.toUtf8() << " (len=" << s2.len() << ")\n";

  EXPECT_EQ(s1, s2);
  EXPECT_EQ(s2.toUtf8(), "مرحبا");
}

TEST(DiagnosticTest, CheckMemoryIndependence)
{
  StringRef s1   = StringRef::fromUtf8("Test");
  CharType* ptr1 = s1.data();

  StringRef s2(s1);
  CharType* ptr2 = s2.data();

  std::cout << "s1 ptr: " << (void*) ptr1 << "\n";
  std::cout << "s2 ptr: " << (void*) ptr2 << "\n";

  EXPECT_NE(ptr1, ptr2) << "Copy constructor should allocate new memory!";

  // Modify through s1's pointer
  ptr1[0] = 'X';

  // s2 should be unaffected
  EXPECT_NE(s1[0], s2[0]);
  EXPECT_EQ(s1[0], 'X');
  EXPECT_EQ(s2[0], 'T');
}

TEST_F(StringRefTest, CopyConstructor_NonEmpty)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2(s1);
  EXPECT_EQ(s2.len(), s1.len());
  EXPECT_EQ(s2, s1);
  EXPECT_NE(s2.data(), s1.data());  // Different memory
}

TEST_F(StringRefTest, CopyConstructor_Arabic)
{
  StringRef s1 = StringRef::fromUtf8("مرحبا");
  StringRef s2(s1);
  EXPECT_EQ(s2, s1);
  EXPECT_EQ(s2.toUtf8(), "مرحبا");
}
/*
TEST_F(StringRefTest, MoveConstructor_NonEmpty)
{
  StringRef s1      = StringRef::fromUtf8("Hello");
  CharType* old_ptr = s1.data();
  SizeType  old_len = s1.len();
  
  StringRef s2(std::move(s1));
  
  EXPECT_EQ(s2.len(), old_len);
  EXPECT_EQ(s2.data(), old_ptr);
  EXPECT_TRUE(s1.empty());  // s1 should be empty
  EXPECT_EQ(s1.data(), nullptr);
}
*/

TEST_F(StringRefTest, CStyleConstructor_Null)
{
  const CharType* null_ptr = nullptr;
  StringRef       s(null_ptr);
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, CStyleConstructor_Empty)
{
  CharType  empty[] = {CharType{0}};
  StringRef s(empty);
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, CStyleConstructor_NonEmpty)
{
  CharType  hello[] = {'H', 'e', 'l', 'l', 'o', CharType{0}};
  StringRef s(hello);
  EXPECT_EQ(s.len(), 5);
  EXPECT_EQ(s[0], 'H');
  EXPECT_EQ(s[4], 'o');
}

// assign

TEST_F(StringRefTest, CopyAssignment_SelfAssignment)
{
  StringRef s = StringRef::fromUtf8("Test");
  s           = s;  // Self-assignment
  EXPECT_EQ(s.toUtf8(), "Test");
}

TEST_F(StringRefTest, CopyAssignment_EmptyToEmpty)
{
  StringRef s1, s2;
  s2 = s1;
  EXPECT_TRUE(s2.empty());
}

TEST_F(StringRefTest, CopyAssignment_NonEmptyToEmpty)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  std::cout << "s1 before assign : " << s1 << std::endl;
  StringRef s2;
  s2 = s1;
  std::cout << "s1 after assign  : " << s1 << std::endl;
  std::cout << "s2               : " << s2 << std::endl;
  EXPECT_EQ(s2, s1);
}

TEST_F(StringRefTest, CopyAssignment_EmptyToNonEmpty)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2;
  s1 = s2;
  EXPECT_TRUE(s1.empty());
}

TEST_F(StringRefTest, CopyAssignment_ReplaceContent)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8("World");
  s1           = s2;
  EXPECT_EQ(s1.toUtf8(), "World");
}

/*
TEST_F(StringRefTest, MoveAssignment_SelfAssignment)
{
  StringRef s = StringRef::fromUtf8("Test");
  s           = std::move(s);  // Self-assignment
  EXPECT_EQ(s.toUtf8(), "Test");
}

TEST_F(StringRefTest, MoveAssignment_NonEmpty)
{
  StringRef s1      = StringRef::fromUtf8("Hello");
  StringRef s2      = StringRef::fromUtf8("World");
  CharType* old_ptr = s2.data();
  
  s1 = std::move(s2);
  
  EXPECT_EQ(s1.data(), old_ptr);
  EXPECT_EQ(s1.toUtf8(), "World");
  EXPECT_TRUE(s2.empty());
}
*/

TEST_F(StringRefTest, Utf8Assignment_FromCString)
{
  StringRef s;
  s = "Hello";
  EXPECT_EQ(s.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Utf8Assignment_NullPtr)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s           = static_cast<const char*>(nullptr);
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
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8("Hello");
  EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_DifferentContent)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8("World");
  EXPECT_NE(s1, s2);
}

TEST_F(StringRefTest, Equality_DifferentLength)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8("Hi");
  EXPECT_NE(s1, s2);
}

TEST_F(StringRefTest, Equality_SelfComparison)
{
  StringRef s = StringRef::fromUtf8("Test");
  EXPECT_EQ(s, s);
}

TEST_F(StringRefTest, Equality_ArabicText)
{
  StringRef s1 = StringRef::fromUtf8("مرحبا بك");
  StringRef s2 = StringRef::fromUtf8("مرحبا بك");
  EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_ChineseText)
{
  StringRef s1 = StringRef::fromUtf8("你好世界");
  StringRef s2 = StringRef::fromUtf8("你好世界");
  EXPECT_EQ(s1, s2);
}

TEST_F(StringRefTest, Equality_Emoji)
{
  StringRef s1 = StringRef::fromUtf8("Hello 😀🎉");
  StringRef s2 = StringRef::fromUtf8("Hello 😀🎉");
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
  SizeType  old_cap = s.cap();
  s.expand(50);
  EXPECT_EQ(s.cap(), old_cap);  // Should not change
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
  StringRef s = StringRef::fromUtf8("Hello");
  s.reserve(1000);
  EXPECT_EQ(s.toUtf8(), "Hello");
  EXPECT_GE(s.cap(), 1000);
}

TEST_F(StringRefTest, Clear_EmptiesString)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.clear();
  EXPECT_EQ(s.len(), 0);
  EXPECT_TRUE(s.empty());
  EXPECT_GT(s.cap(), 0);  // Capacity should remain
}

TEST_F(StringRefTest, Clear_MultipleTime)
{
  StringRef s = StringRef::fromUtf8("Hello");
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
  EXPECT_EQ(s.toUtf8(), "Hi");
}

TEST_F(StringRefTest, AppendChar_UnicodeChar)
{
  StringRef s;
  s += CharType(0x0645);  // Arabic letter Meem
  EXPECT_EQ(s.len(), 1);
}

TEST_F(StringRefTest, AppendChar_TriggerExpansion)
{
  StringRef s(2);
  for (int i = 0; i < 100; i++)
  {
    s += 'A';
  }
  EXPECT_EQ(s.len(), 100);
}

TEST_F(StringRefTest, AppendString_ToEmpty)
{
  StringRef s1;
  StringRef s2 = StringRef::fromUtf8("Hello");
  s1 += s2;
  EXPECT_EQ(s1.toUtf8(), "Hello");
}

TEST_F(StringRefTest, AppendString_EmptyToNonEmpty)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2;
  s1 += s2;
  EXPECT_EQ(s1.toUtf8(), "Hello");
}

TEST_F(StringRefTest, AppendString_NonEmptyToNonEmpty)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8(" World");
  s1 += s2;
  EXPECT_EQ(s1.toUtf8(), "Hello World");
}

TEST_F(StringRefTest, AppendString_Arabic)
{
  StringRef s1 = StringRef::fromUtf8("مرحبا");
  StringRef s2 = StringRef::fromUtf8(" بك");
  s1 += s2;
  EXPECT_EQ(s1.toUtf8(), "مرحبا بك");
}

TEST_F(StringRefTest, AppendString_TriggerReallocation)
{
  StringRef s1(5);
  StringRef s2 = StringRef::fromUtf8("This is a very long string that will trigger reallocation");
  s1 += s2;
  EXPECT_EQ(s1, s2);
}

// concatenation

TEST_F(StringRefTest, Concat_StringPlusString)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = StringRef::fromUtf8(" World");
  StringRef s3 = s1 + s2;
  EXPECT_EQ(s3.toUtf8(), "Hello World");
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
  StringRef s2 = StringRef::fromUtf8("Hello");
  StringRef s3 = s1 + s2;
  EXPECT_EQ(s3.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Concat_StringPlusCString)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = s1 + " World";
  EXPECT_EQ(s2.toUtf8(), "Hello World");
}

TEST_F(StringRefTest, Concat_CStringPlusString)
{
  StringRef s1 = StringRef::fromUtf8(" World");
  StringRef s2 = "Hello" + s1;
  EXPECT_EQ(s2.toUtf8(), "Hello World");
}

TEST_F(StringRefTest, Concat_StringPlusChar)
{
  StringRef s1 = StringRef::fromUtf8("Hell");
  StringRef s2 = s1 + CharType('o');
  EXPECT_EQ(s2.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Concat_CharPlusString)
{
  StringRef s1 = StringRef::fromUtf8("ello");
  StringRef s2 = CharType('H') + s1;
  EXPECT_EQ(s2.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Concat_Chain)
{
  StringRef s1     = StringRef::fromUtf8("A");
  StringRef s2     = StringRef::fromUtf8("B");
  StringRef s3     = StringRef::fromUtf8("C");
  StringRef result = s1 + s2 + s3;
  EXPECT_EQ(result.toUtf8(), "ABC");
}

TEST_F(StringRefTest, Concat_NullCString)
{
  StringRef s1 = StringRef::fromUtf8("Hello");
  StringRef s2 = s1 + static_cast<const char*>(nullptr);
  EXPECT_EQ(s2.toUtf8(), "Hello");
}

// index access

TEST_F(StringRefTest, IndexOperator_ValidAccess)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_EQ(s[0], 'H');
  EXPECT_EQ(s[4], 'o');
}

TEST_F(StringRefTest, IndexOperator_Mutable)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s[0]        = 'J';
  EXPECT_EQ(s[0], 'J');
}

TEST_F(StringRefTest, At_ValidAccess)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_EQ(s.at(0), 'H');
  EXPECT_EQ(s.at(4), 'o');
}

TEST_F(StringRefTest, At_OutOfBounds)
{
  StringRef s = StringRef::fromUtf8("Hi");
  EXPECT_THROW(s.at(10), std::out_of_range);
}

TEST_F(StringRefTest, At_MutableAccess)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.at(0)     = 'J';
  EXPECT_EQ(s.at(0), 'J');
}

TEST_F(StringRefTest, At_EmptyString)
{
  StringRef s;
  EXPECT_THROW(s.at(0), std::runtime_error);
}

// erase

TEST_F(StringRefTest, Erase_FirstChar)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.erase(0);
  EXPECT_EQ(s.toUtf8(), "ello");
}

TEST_F(StringRefTest, Erase_LastChar)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.erase(4);
  EXPECT_EQ(s.toUtf8(), "Hell");
}

TEST_F(StringRefTest, Erase_MiddleChar)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.erase(2);
  EXPECT_EQ(s.toUtf8(), "Helo");
}

TEST_F(StringRefTest, Erase_OutOfBounds)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.erase(100);  // Should do nothing
  EXPECT_EQ(s.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Erase_EmptyString)
{
  StringRef s;
  s.erase(0);  // Should do nothing
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Erase_AllChars)
{
  StringRef s = StringRef::fromUtf8("Hi");
  s.erase(0);
  s.erase(0);
  EXPECT_TRUE(s.empty());
}

// find

TEST_F(StringRefTest, Find_CharExists)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_TRUE(s.find('H'));
  EXPECT_TRUE(s.find('o'));
}

TEST_F(StringRefTest, Find_CharNotExists)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_FALSE(s.find('X'));
}

TEST_F(StringRefTest, Find_EmptyString)
{
  StringRef s;
  EXPECT_FALSE(s.find('A'));
}

TEST_F(StringRefTest, FindPos_CharExists)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  auto      pos = s.find_pos('e');
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos.value(), 1);
}

TEST_F(StringRefTest, FindPos_CharNotExists)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  auto      pos = s.find_pos('X');
  EXPECT_FALSE(pos.has_value());
}

TEST_F(StringRefTest, FindPos_FirstOccurrence)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  auto      pos = s.find_pos('l');
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(pos.value(), 2);  // First 'l'
}

// truncate

TEST_F(StringRefTest, Truncate_ToShorter)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.truncate(3);
  EXPECT_EQ(s.len(), 3);
  EXPECT_EQ(s.toUtf8(), "Hel");
}

TEST_F(StringRefTest, Truncate_ToZero)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.truncate(0);
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, Truncate_ToLargerSize)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s.truncate(100);  // Should do nothing
  EXPECT_EQ(s.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Truncate_EmptyString)
{
  StringRef s;
  s.truncate(5);  // Should do nothing
  EXPECT_TRUE(s.empty());
}

// substring

TEST_F(StringRefTest, Substr_FullString)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  StringRef sub = s.substr(0, 4);
  EXPECT_EQ(sub.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Substr_MiddlePortion)
{
  StringRef s   = StringRef::fromUtf8("Hello World");
  StringRef sub = s.substr(6, 10);
  EXPECT_EQ(sub.toUtf8(), "World");
}

TEST_F(StringRefTest, Substr_FirstChar)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  StringRef sub = s.substr(0, 0);
  EXPECT_EQ(sub.len(), 1);
  EXPECT_EQ(sub[0], 'H');
}

TEST_F(StringRefTest, Substr_LastChar)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  StringRef sub = s.substr(4, 4);
  EXPECT_EQ(sub.len(), 1);
  EXPECT_EQ(sub[0], 'o');
}

TEST_F(StringRefTest, Substr_DefaultEnd)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  StringRef sub = s.substr(2);
  EXPECT_EQ(sub.toUtf8(), "llo");
}

TEST_F(StringRefTest, Substr_EmptyString)
{
  StringRef s;
  StringRef sub = s.substr(0, 0);
  EXPECT_TRUE(sub.empty());
}

TEST_F(StringRefTest, Substr_StartOutOfBounds)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_THROW(s.substr(100, 100), std::out_of_range);
}

TEST_F(StringRefTest, Substr_EndLargerThanLength)
{
  StringRef s   = StringRef::fromUtf8("Hello");
  StringRef sub = s.substr(0, 100);  // Should clamp to length
  EXPECT_EQ(sub.toUtf8(), "Hello");
}

TEST_F(StringRefTest, Substr_EndBeforeStart)
{
  StringRef s = StringRef::fromUtf8("Hello");
  EXPECT_THROW(s.substr(4, 2), std::invalid_argument);
}

TEST_F(StringRefTest, Substr_ArabicText)
{
  StringRef s   = StringRef::fromUtf8("مرحبا بك في العالم");
  StringRef sub = s.substr(0, 4);
  EXPECT_EQ(sub.toUtf8(), "مرحبا");
}

// UTF-8 conversion

TEST_F(StringRefTest, FromUtf8_EmptyString)
{
  StringRef s = StringRef::fromUtf8("");
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, FromUtf8_AsciiString)
{
  StringRef s = StringRef::fromUtf8("Hello World");
  EXPECT_EQ(s.toUtf8(), "Hello World");
}

TEST_F(StringRefTest, FromUtf8_ArabicString)
{
  StringRef s = StringRef::fromUtf8("مرحبا");
  EXPECT_EQ(s.toUtf8(), "مرحبا");
}

TEST_F(StringRefTest, FromUtf8_ChineseString)
{
  StringRef s = StringRef::fromUtf8("你好世界");
  EXPECT_EQ(s.toUtf8(), "你好世界");
}

TEST_F(StringRefTest, FromUtf8_JapaneseString)
{
  StringRef s = StringRef::fromUtf8("こんにちは");
  EXPECT_EQ(s.toUtf8(), "こんにちは");
}

TEST_F(StringRefTest, FromUtf8_RussianString)
{
  StringRef s = StringRef::fromUtf8("Привет мир");
  EXPECT_EQ(s.toUtf8(), "Привет мир");
}

TEST_F(StringRefTest, FromUtf8_EmojiString)
{
  StringRef s = StringRef::fromUtf8("Hello 😀 World 🎉");
  EXPECT_EQ(s.toUtf8(), "Hello 😀 World 🎉");
}

TEST_F(StringRefTest, FromUtf8_MixedScript)
{
  StringRef s = StringRef::fromUtf8("Hello مرحبا 你好 🌍");
  EXPECT_EQ(s.toUtf8(), "Hello مرحبا 你好 🌍");
}

TEST_F(StringRefTest, FromUtf8_NullPtr)
{
  StringRef s = StringRef::fromUtf8(static_cast<const char*>(nullptr));
  EXPECT_TRUE(s.empty());
}

TEST_F(StringRefTest, FromUtf8_StdString)
{
  std::string utf8 = "Test String";
  StringRef   s    = StringRef::fromUtf8(utf8);
  EXPECT_EQ(s.toUtf8(), utf8);
}

TEST_F(StringRefTest, ToUtf8_EmptyString)
{
  StringRef s;
  EXPECT_EQ(s.toUtf8(), "");
}

TEST_F(StringRefTest, ToUtf8_RoundTrip)
{
  std::string original = "Hello World مرحبا 你好 😀";
  StringRef   s        = StringRef::fromUtf8(original);
  std::string result   = s.toUtf8();
  EXPECT_EQ(result, original);
}

// to double

TEST_F(StringRefTest, ToDouble_Integer)
{
  StringRef s = StringRef::fromUtf8("42");
  EXPECT_DOUBLE_EQ(s.toDouble(), 42.0);
}

TEST_F(StringRefTest, ToDouble_Float)
{
  StringRef s = StringRef::fromUtf8("3.14159");
  EXPECT_DOUBLE_EQ(s.toDouble(), 3.14159);
}

TEST_F(StringRefTest, ToDouble_Negative)
{
  StringRef s = StringRef::fromUtf8("-123.456");
  EXPECT_DOUBLE_EQ(s.toDouble(), -123.456);
}

TEST_F(StringRefTest, ToDouble_Scientific)
{
  StringRef s = StringRef::fromUtf8("1.5e10");
  EXPECT_DOUBLE_EQ(s.toDouble(), 1.5e10);
}

TEST_F(StringRefTest, ToDouble_EmptyString)
{
  StringRef s;
  EXPECT_THROW(s.toDouble(), std::invalid_argument);
}

TEST_F(StringRefTest, ToDouble_InvalidFormat)
{
  StringRef s = StringRef::fromUtf8("not a number");
  EXPECT_THROW(s.toDouble(), std::invalid_argument);
}

TEST_F(StringRefTest, ToDouble_WithPosition)
{
  StringRef s      = StringRef::fromUtf8("123.456abc");
  SizeType  pos    = 0;
  double    result = s.toDouble(&pos);
  EXPECT_DOUBLE_EQ(result, 123.456);
  EXPECT_GT(pos, 0);
}

// hash

TEST_F(StringRefTest, Hash_EmptyString)
{
  StringRefHash hasher;
  StringRef     s;
  EXPECT_EQ(hasher(s), 0);
}

TEST_F(StringRefTest, Hash_NonEmpty)
{
  StringRefHash hasher;
  StringRef     s    = StringRef::fromUtf8("Hello");
  size_t        hash = hasher(s);
  EXPECT_NE(hash, 0);
}

TEST_F(StringRefTest, Hash_SameContent)
{
  StringRefHash hasher;
  StringRef     s1 = StringRef::fromUtf8("Hello");
  StringRef     s2 = StringRef::fromUtf8("Hello");
  EXPECT_EQ(hasher(s1), hasher(s2));
}

TEST_F(StringRefTest, Hash_DifferentContent)
{
  StringRefHash hasher;
  StringRef     s1 = StringRef::fromUtf8("Hello");
  StringRef     s2 = StringRef::fromUtf8("World");
  EXPECT_NE(hasher(s1), hasher(s2));
}

TEST_F(StringRefTest, StdHash_Works)
{
  std::hash<StringRef> hasher;
  StringRef            s    = StringRef::fromUtf8("Test");
  size_t               hash = hasher(s);
  EXPECT_NE(hash, 0);
}

// stress tests

TEST_F(StringRefTest, Stress_ManyAppends)
{
  StringRef s;
  for (int i = 0; i < 10000; i++)
  {
    s += CharType('A' + (i % 26));
  }
  EXPECT_EQ(s.len(), 10000);
}

TEST_F(StringRefTest, Stress_ManyErases)
{
  StringRef s;
  for (int i = 0; i < 1000; i++)
  {
    s += CharType('A');
  }
  for (int i = 0; i < 500; i++)
  {
    s.erase(0);
  }
  EXPECT_EQ(s.len(), 500);
}

TEST_F(StringRefTest, Stress_CopyAndModify)
{
  StringRef              original = StringRef::fromUtf8("Original");
  std::vector<StringRef> copies;

  for (int i = 0; i < 100; i++)
  {
    copies.push_back(original);
    copies.back() += CharType('0' + i % 10);
  }

  EXPECT_EQ(original.toUtf8(), "Original");
  for (size_t i = 0; i < copies.size(); i++)
  {
    EXPECT_NE(copies[i], original);
  }
}

TEST_F(StringRefTest, Stress_LargeString)
{
  std::string large(100000, 'A');
  StringRef   s = StringRef::fromUtf8(large);
  EXPECT_EQ(s.len(), 100000);
  // EXPECT_EQ(s.toUtf8(), large);
}

TEST_F(StringRefTest, Stress_UnicodeAppends)
{
  StringRef s;
  for (int i = 0; i < 1000; i++)
  {
    s = s + StringRef::fromUtf8("مرحبا");
  }
  EXPECT_EQ(s.len(), 5000);  // Each "مرحبا" is 5 characters
}

TEST_F(StringRefTest, Stress_ManySubstrings)
{
  StringRef              s = StringRef::fromUtf8("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  std::vector<StringRef> subs;

  for (SizeType i = 0; i < s.len(); i++)
  {
    for (SizeType j = i; j < s.len(); j++)
    {
      subs.push_back(s.substr(i, j));
    }
  }

  EXPECT_GT(subs.size(), 100);
}

TEST_F(StringRefTest, Stress_RandomOperations)
{
  std::mt19937                    rng(42);
  std::uniform_int_distribution<> op_dist(0, 4);
  std::uniform_int_distribution<> char_dist('A', 'Z');

  StringRef s;

  for (int i = 0; i < 1000; i++)
  {
    int op = op_dist(rng);
    switch (op)
    {
    case 0 :  // Append
      s += CharType(char_dist(rng));
      break;
    case 1 :  // Clear
      s.clear();
      break;
    case 2 :  // Erase
      if (!s.empty())
      {
        s.erase(0);
      }
      break;
    case 3 :  // Truncate
      if (!s.empty())
      {
        s.truncate(s.len() / 2);
      }
      break;
    case 4 :  // Copy
    {
      StringRef copy = s;
      s              = copy;
    }
    break;
    }
  }

  // Should not crash
  EXPECT_TRUE(true);
}

// edge cases

TEST_F(StringRefTest, EdgeCase_NullTerminatorInMiddle)
{
  StringRef s = StringRef::fromUtf8("Hello");
  s[2]        = CharType{0};
  // toUtf8 should handle this correctly (might stop at null or not)
  EXPECT_LE(s.toUtf8().length(), 5);
}

TEST_F(StringRefTest, EdgeCase_MaxSizeString)
{
  // Test with a reasonably large string
  const size_t large_size = 1000000;
  StringRef    s(large_size);
  for (size_t i = 0; i < 100; i++)
  {
    s += 'A';
  }
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
  StringRef s = StringRef::fromUtf8("😀😃😄😁");
  EXPECT_GT(s.len(), 4);  // Each emoji is 2 UTF-16 code units
}

TEST_F(StringRefTest, EdgeCase_AllZeros)
{
  StringRef s(10);
  for (int i = 0; i < 10; i++)
  {
    s += CharType{0};
  }
  EXPECT_EQ(s.len(), 10);
}

TEST_F(StringRefTest, EdgeCase_HighUnicodeValues)
{
  // Test high code points
  StringRef s;
  s += CharType(0xD800);  // High surrogate start
                          // This might be invalid, but shouldn't crash
}

// memory leaks

TEST_F(StringRefTest, NoLeak_MultipleConstructDestruct)
{
  for (int i = 0; i < 1000; i++)
  {
    StringRef s = StringRef::fromUtf8("Test String");
  }
  // Valgrind or AddressSanitizer will catch leaks
  EXPECT_TRUE(true);
}

TEST_F(StringRefTest, NoLeak_CopyAssignmentLoop)
{
  StringRef original = StringRef::fromUtf8("Original");
  for (int i = 0; i < 100; i++)
  {
    StringRef copy;
    copy = original;
  }
  EXPECT_TRUE(true);
}

/*
TEST_F(StringRefTest, NoLeak_MoveAssignmentLoop)
{
  for (int i = 0; i < 100; i++)
  {
    StringRef s1 = StringRef::fromUtf8("Test");
    StringRef s2 = s1;
  }
  EXPECT_TRUE(true);
}
*/

// perf

TEST_F(StringRefTest, Performance_AppendChars)
{
  auto start = std::chrono::high_resolution_clock::now();

  StringRef s;
  s.reserve(10000);
  for (int i = 0; i < 10000; i++)
  {
    s += CharType('A');
  }

  auto end      = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(s.len(), 10000);
  std::cout << "Append 10000 chars took: " << duration.count() << "ms\n";
}

TEST_F(StringRefTest, Performance_Concatenation)
{
  auto start = std::chrono::high_resolution_clock::now();

  StringRef result;
  StringRef part = StringRef::fromUtf8("Part");

  for (int i = 0; i < 1000; i++)
  {
    result = result + part;
  }

  auto end      = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(result.len(), 4000);
  std::cout << "1000 concatenations took: " << duration.count() << "ms\n";
}

TEST_F(StringRefTest, Performance_Utf8Conversion)
{
  std::string utf8(10000, 'A');

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; i++)
  {
    StringRef   s    = StringRef::fromUtf8(utf8);
    std::string back = s.toUtf8();
  }

  auto end      = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "100 UTF-8 round-trips (10k chars) took: " << duration.count() << "ms\n";
}
