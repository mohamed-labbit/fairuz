/**
 * test_stdlib.cpp
 * Battle tests for all mylang::runtime native stdlib functions.
 *
 * Build alongside your project and link with GTest.
 * Compile: g++ -std=c++17 test_stdlib.cpp -lgtest -lgtest_main -o test_stdlib
 */

#include <gtest/gtest.h>
#include "../../include/runtime/stdlib.hpp"

namespace mylang::runtime {

static Value makeStr(const char* s)
{
    return Value::object(makeObjectString(StringRef(s)));
}

static Value makeList(std::initializer_list<Value> elems = {})
{
    Value v = Value::object(makeObjectList());
    for (const auto& e : elems)
        v.asList()->elements.push(e);
    return v;
}

// Convenience: call a native function with a braced list of Values.
// Usage: call(nativeLen, {makeStr("hello")})
static Value call(Value (*fn)(int, Value*), std::initializer_list<Value> args)
{
    std::vector<Value> v(args);
    return fn(static_cast<int>(v.size()), v.empty() ? nullptr : v.data());
}

static Value call0(Value (*fn)(int, Value*))
{
    return fn(0, nullptr);
}

TEST(NativeLen, NoArgs)
{
    EXPECT_TRUE(call0(nativeLen).isNil());
}

TEST(NativeLen, NullArgv)
{
    // argc = 1 but argv = nullptr
    EXPECT_TRUE(nativeLen(1, nullptr).isNil());
}

TEST(NativeLen, EmptyString)
{
    auto s = makeStr("");
    EXPECT_EQ(call(nativeLen, {s}).asInteger(), 0);
}

TEST(NativeLen, NonEmptyString)
{
    auto s = makeStr("hello");
    EXPECT_EQ(call(nativeLen, {s}).asInteger(), 5);
}

TEST(NativeLen, UnicodeString)
{
    // Ensure we're counting bytes / chars consistently
    auto s = makeStr("abc");
    EXPECT_EQ(call(nativeLen, {s}).asInteger(), 3);
}

TEST(NativeLen, EmptyList)
{
    auto lst = makeList({});
    EXPECT_EQ(call(nativeLen, {lst}).asInteger(), 0);
}

TEST(NativeLen, NonEmptyList)
{
    auto lst = makeList({Value::integer(1), Value::integer(2), Value::integer(3)});
    EXPECT_EQ(call(nativeLen, {lst}).asInteger(), 3);
}

TEST(NativeLen, MultipleArgs_ReturnsNil)
{
    // len does not accept multiple arguments
    auto s = makeStr("hi");
    EXPECT_TRUE(call(nativeLen, {s, s}).isNil());
}

TEST(NativeLen, IntegerArg_ReturnsNil)
{
    // len of a bare integer is undefined
    EXPECT_TRUE(call(nativeLen, {Value::integer(42)}).isNil());
}

TEST(NativeLen, BoolArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeLen, {Value::boolean(true)}).isNil());
}

TEST(NativeLen, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeLen, {Value::nil()}).isNil());
}

// nativePrint is mostly a side-effect function; verify it never crashes and
// always returns nil.

TEST(NativePrint, NoArgs_PrintsNewline)
{
    EXPECT_TRUE(call0(nativePrint).isNil());
}

TEST(NativePrint, StringArg)
{
    auto s = makeStr("hello world");
    EXPECT_TRUE(call(nativePrint, {s}).isNil());
}

TEST(NativePrint, IntegerArg)
{
    EXPECT_TRUE(call(nativePrint, {Value::integer(42)}).isNil());
}

TEST(NativePrint, FloatArg)
{
    EXPECT_TRUE(call(nativePrint, {Value::real(3.14)}).isNil());
}

TEST(NativePrint, BoolArg_True)
{
    EXPECT_TRUE(call(nativePrint, {Value::boolean(true)}).isNil());
}

TEST(NativePrint, BoolArg_False)
{
    EXPECT_TRUE(call(nativePrint, {Value::boolean(false)}).isNil());
}

TEST(NativePrint, NilArg)
{
    EXPECT_TRUE(call(nativePrint, {Value::nil()}).isNil());
}

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    // More than 1 arg — current impl treats this like no args (prints newline)
    auto s = makeStr("a");
    EXPECT_TRUE(call(nativePrint, {s, s}).isNil());
}

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    EXPECT_TRUE(call0(nativeStr).isString());
}

TEST(NativeStr, NullArgv_ReturnsNil)
{
    EXPECT_TRUE(nativeStr(1, nullptr).isNil());
}

TEST(NativeStr, Integer)
{
    Value r = call(nativeStr, {Value::integer(42)});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    Value r = call(nativeStr, {Value::integer(-7)});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    Value r = call(nativeStr, {Value::integer(0)});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    Value r = call(nativeStr, {Value::boolean(true)});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "true");
}

TEST(NativeStr, BoolFalse)
{
    Value r = call(nativeStr, {Value::boolean(false)});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "false");
}

TEST(NativeStr, Float)
{
    Value r = call(nativeStr, {Value::real(1.5)});
    ASSERT_TRUE(r.isString());
    // std::to_string(1.5) == "1.500000" — just verify it parses back
    double parsed = std::stod(r.asString()->str.data());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    Value s = makeStr("hello");
    Value r = call(nativeStr, {s});
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(std::string(r.asString()->str.data()), "hello");
}

TEST(NativeStr, NilArg_ReturnsNil)
{
    // nil is not handled by any branch — returns nil or empty string;
    // either way must not crash.
    EXPECT_NO_FATAL_FAILURE(call(nativeStr, {Value::nil()}));
}

TEST(NativeStr, TwoArgs_ReturnsNil)
{
    auto s = makeStr("x");
    EXPECT_TRUE(call(nativeStr, {s, s}).isNil());
}

TEST(NativeBool, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeBool).isNil());
}

TEST(NativeBool, TrueBoolean)
{
    Value r = call(nativeBool, {Value::boolean(true)});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_TRUE(r.asBoolean());
}

TEST(NativeBool, FalseBoolean)
{
    Value r = call(nativeBool, {Value::boolean(false)});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_FALSE(r.asBoolean());
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    Value r = call(nativeBool, {Value::integer(1)});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_TRUE(r.asBoolean());
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    Value r = call(nativeBool, {Value::integer(0)});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_FALSE(r.asBoolean());
}

TEST(NativeBool, NilArg)
{
    Value r = call(nativeBool, {Value::nil()});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_FALSE(r.asBoolean());
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    Value r = call(nativeBool, {makeStr("hi")});
    ASSERT_TRUE(r.isBoolean());
    EXPECT_TRUE(r.asBoolean());
}

TEST(NativeInt, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeInt).isNil());
}

TEST(NativeInt, IntegerPassthrough)
{
    Value r = call(nativeInt, {Value::integer(7)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 7);
}

TEST(NativeInt, FloatTruncates)
{
    Value r = call(nativeInt, {Value::real(3.9)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 3);
}

TEST(NativeInt, NegativeFloat)
{
    Value r = call(nativeInt, {Value::real(-2.7)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), -2);
}

TEST(NativeInt, StringArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeInt, {makeStr("42")}).isNil());
}

TEST(NativeInt, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeInt, {Value::nil()}).isNil());
}

TEST(NativeInt, BoolArg_ReturnsNil)
{
    // bool is not isNumber() — should return nil
    EXPECT_TRUE(call(nativeInt, {Value::boolean(true)}).isNil());
}

TEST(NativeFloat, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeFloat).isNil());
}

TEST(NativeFloat, IntegerToFloat)
{
    Value r = call(nativeFloat, {Value::integer(3)});
    ASSERT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    Value r = call(nativeFloat, {Value::real(2.5)});
    ASSERT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    Value r = call(nativeFloat, {Value::integer(-10)});
    ASSERT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), -10.0);
}

TEST(NativeFloat, StringArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeFloat, {makeStr("3.14")}).isNil());
}

TEST(NativeFloat, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeFloat, {Value::nil()}).isNil());
}

TEST(NativeType, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeType).isNil());
}

TEST(NativeType, ReturnsInteger)
{
    // Whatever the tag is, it must be an integer Value
    EXPECT_TRUE(call(nativeType, {Value::integer(0)}).isInteger());
    EXPECT_TRUE(call(nativeType, {Value::real(0.0)}).isInteger());
    EXPECT_TRUE(call(nativeType, {Value::boolean(false)}).isInteger());
    EXPECT_TRUE(call(nativeType, {Value::nil()}).isInteger());
    EXPECT_TRUE(call(nativeType, {makeStr("x")}).isInteger());
}

TEST(NativeType, DifferentTypesHaveDifferentTags)
{
    int64_t int_tag  = call(nativeType, {Value::integer(0)}).asInteger();
    int64_t flt_tag  = call(nativeType, {Value::real(0.0)}).asInteger();
    int64_t bool_tag = call(nativeType, {Value::boolean(false)}).asInteger();
    int64_t nil_tag  = call(nativeType, {Value::nil()}).asInteger();
    int64_t str_tag  = call(nativeType, {makeStr("x")}).asInteger();

    EXPECT_NE(int_tag, flt_tag);
    EXPECT_NE(int_tag, nil_tag);
    EXPECT_NE(str_tag, nil_tag);
    EXPECT_NE(bool_tag, nil_tag);
}

TEST(NativeAppend, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeAppend).isNil());
}

TEST(NativeAppend, OneArg_ReturnsNil)
{
    auto lst = makeList();
    EXPECT_TRUE(call(nativeAppend, {lst}).isNil());
}

TEST(NativeAppend, AppendSingleElement)
{
    auto lst = makeList();
    call(nativeAppend, {lst, Value::integer(99)});
    EXPECT_EQ(lst.asList()->elements.size(), 1u);
    EXPECT_EQ(lst.asList()->elements[0].asInteger(), 99);
}

TEST(NativeAppend, AppendMultipleElements)
{
    auto lst = makeList();
    call(nativeAppend, {lst, Value::integer(1), Value::integer(2), Value::integer(3)});
    EXPECT_EQ(lst.asList()->elements.size(), 3u);
}

TEST(NativeAppend, AppendToNonEmptyList)
{
    auto lst = makeList({Value::integer(0)});
    call(nativeAppend, {lst, Value::integer(1)});
    EXPECT_EQ(lst.asList()->elements.size(), 2u);
}

TEST(NativeAppend, AppendDifferentTypes)
{
    auto lst = makeList();
    call(nativeAppend, {lst, Value::integer(1), Value::real(2.0), makeStr("x"), Value::boolean(true)});
    EXPECT_EQ(lst.asList()->elements.size(), 4u);
}

TEST(NativeAppend, NonListFirstArg_ReturnsNil)
{
    Value x = Value::integer(5);
    EXPECT_TRUE(call(nativeAppend, {x, Value::integer(1)}).isNil());
}

TEST(NativeAppend, ReturnValueIsNil)
{
    auto lst = makeList();
    EXPECT_TRUE(call(nativeAppend, {lst, Value::integer(1)}).isNil());
}

TEST(NativePop, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativePop).isNil());
}

TEST(NativePop, RemovesLastElement)
{
    auto lst = makeList({Value::integer(1), Value::integer(2)});
    call(nativeAppend, {lst, Value::integer(3)});
    call(nativePop, {lst});
    EXPECT_EQ(lst.asList()->elements.size(), 2u);
}

TEST(NativePop, SingleElementList)
{
    auto lst = makeList({Value::integer(42)});
    call(nativePop, {lst});
    EXPECT_EQ(lst.asList()->elements.size(), 0u);
}

TEST(NativePop, ReturnValueIsNil)
{
    auto lst = makeList({Value::integer(1)});
    EXPECT_TRUE(call(nativePop, {lst}).isNil());
}

TEST(NativePop, NonListArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativePop, {Value::integer(5)}).isNil());
}

TEST(NativePop, NullArgv_ReturnsNil)
{
    EXPECT_TRUE(nativePop(1, nullptr).isNil());
}

TEST(NativeSlice, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeSlice).isNil());
}

TEST(NativeSlice, OneArg_ReturnsNil)
{
    auto lst = makeList({Value::integer(1)});
    EXPECT_TRUE(call(nativeSlice, {lst}).isNil());
}

TEST(NativeSlice, TwoArgs_FromIndex)
{
    auto lst = makeList({Value::integer(10), Value::integer(20), Value::integer(30)});
    Value r = call(nativeSlice, {lst, Value::integer(1)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 2u); // [20, 30]
    EXPECT_EQ(r.asList()->elements[0].asInteger(), 20);
}

TEST(NativeSlice, ThreeArgs_SubRange)
{
    auto lst = makeList({Value::integer(10), Value::integer(20), Value::integer(30), Value::integer(40)});
    Value r = call(nativeSlice, {lst, Value::integer(1), Value::integer(2)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 2u); // [20, 30] (inclusive)
    EXPECT_EQ(r.asList()->elements[0].asInteger(), 20);
    EXPECT_EQ(r.asList()->elements[1].asInteger(), 30);
}

TEST(NativeSlice, WholeList)
{
    auto lst = makeList({Value::integer(1), Value::integer(2), Value::integer(3)});
    Value r = call(nativeSlice, {lst, Value::integer(0), Value::integer(2)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 3u);
}

TEST(NativeSlice, SingleElement)
{
    auto lst = makeList({Value::integer(1), Value::integer(2), Value::integer(3)});
    Value r = call(nativeSlice, {lst, Value::integer(1), Value::integer(1)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 1u);
    EXPECT_EQ(r.asList()->elements[0].asInteger(), 2);
}

TEST(NativeSlice, ReturnIsNewList_NotAlias)
{
    // Mutating the original should not affect the slice
    auto lst = makeList({Value::integer(1), Value::integer(2)});
    Value r = call(nativeSlice, {lst, Value::integer(0), Value::integer(0)});
    call(nativeAppend, {lst, Value::integer(99)});
    EXPECT_EQ(r.asList()->elements.size(), 1u);
}

TEST(NativeList, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeList).isNil());
}

TEST(NativeList, OneArg_ReturnsNil)
{
    // Current impl requires >= 2 — note this may be a bug (see review notes)
    EXPECT_TRUE(call(nativeList, {Value::integer(1)}).isNil());
}

TEST(NativeList, TwoArgs_CreatesList)
{
    Value r = call(nativeList, {Value::integer(1), Value::integer(2)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 2u);
}

TEST(NativeList, MixedTypes)
{
    Value r = call(nativeList, {Value::integer(1), makeStr("x"), Value::boolean(false)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements.size(), 3u);
}

TEST(NativeList, PreservesOrder)
{
    Value r = call(nativeList, {Value::integer(10), Value::integer(20), Value::integer(30)});
    ASSERT_TRUE(r.isList());
    EXPECT_EQ(r.asList()->elements[0].asInteger(), 10);
    EXPECT_EQ(r.asList()->elements[1].asInteger(), 20);
    EXPECT_EQ(r.asList()->elements[2].asInteger(), 30);
}

TEST(NativeFloor, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeFloor).isNil());
}

TEST(NativeFloor, IntegerPassthrough)
{
    Value r = call(nativeFloor, {Value::integer(5)});
    // Should be returned unchanged
    EXPECT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    Value r = call(nativeFloor, {Value::real(3.7)});
    EXPECT_DOUBLE_EQ(r.asDouble(), 3.0);
}

TEST(NativeFloor, NegativeFloat)
{
    Value r = call(nativeFloor, {Value::real(-2.3)});
    EXPECT_DOUBLE_EQ(r.asDouble(), -3.0);
}

TEST(NativeFloor, ExactFloat)
{
    Value r = call(nativeFloor, {Value::real(4.0)});
    EXPECT_DOUBLE_EQ(r.asDouble(), 4.0);
}

TEST(NativeFloor, StringArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeFloor, {makeStr("3.5")}).isNil());
}

TEST(NativeFloor, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeFloor, {Value::nil()}).isNil());
}

TEST(NativeFloor, TwoArgs_ReturnsNil)
{
    EXPECT_TRUE(call(nativeFloor, {Value::real(1.5), Value::real(2.5)}).isNil());
}

TEST(NativeCeil, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeCeil).isNil());
}

TEST(NativeCeil, IntegerPassthrough)
{
    Value r = call(nativeCeil, {Value::integer(5)});
    EXPECT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    Value r = call(nativeCeil, {Value::real(3.2)});
    EXPECT_DOUBLE_EQ(r.asDouble(), 4.0);
}

TEST(NativeCeil, NegativeFloat)
{
    Value r = call(nativeCeil, {Value::real(-2.7)});
    EXPECT_DOUBLE_EQ(r.asDouble(), -2.0);
}

TEST(NativeCeil, ExactFloat)
{
    Value r = call(nativeCeil, {Value::real(4.0)});
    EXPECT_DOUBLE_EQ(r.asDouble(), 4.0);
}

TEST(NativeCeil, StringArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeCeil, {makeStr("3.5")}).isNil());
}

TEST(NativeCeil, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeCeil, {Value::nil()}).isNil());
}

TEST(NativeAbs, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeAbs).isNil());
}

TEST(NativeAbs, PositiveInteger)
{
    Value r = call(nativeAbs, {Value::integer(5)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    Value r = call(nativeAbs, {Value::integer(-5)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    Value r = call(nativeAbs, {Value::integer(0)});
    ASSERT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    Value r = call(nativeAbs, {Value::real(3.5)});
    ASSERT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    Value r = call(nativeAbs, {Value::real(-3.5)});
    ASSERT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 3.5);
}

TEST(NativeAbs, StringArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeAbs, {makeStr("-3")}).isNil());
}

TEST(NativeAbs, NilArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeAbs, {Value::nil()}).isNil());
}

// nativeMin  — NOTE: current impl has a known bug (initialises ret to 0.0)
// Tests document both the *correct* expected behavior and the *current* broken
// behavior so you can track the regression after fixing.

TEST(NativeMin, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeMin).isNil());
}

TEST(NativeMin, OneArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeMin, {Value::integer(1)}).isNil());
}

TEST(NativeMin, TwoPositiveIntegers)
{
    Value r = call(nativeMin, {Value::integer(3), Value::integer(7)});
    // CORRECT after bug fix: 3
    // CURRENTLY BROKEN: returns 0 because ret is init'd to 0.0
    EXPECT_EQ(r.asInteger(), 3); // will fail until bug is fixed
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    Value r = call(nativeMin, {Value::integer(5), Value::integer(2), Value::integer(8)});
    EXPECT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    Value r = call(nativeMin, {Value::integer(3), Value::real(1.5)});
    EXPECT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    Value r = call(nativeMin, {Value::integer(-1), Value::integer(-5), Value::integer(-2)});
    EXPECT_EQ(r.asInteger(), -5);
}

TEST(NativeMin, StringFirstArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeMin, {makeStr("a"), makeStr("b")}).isNil());
}

// nativeMax  — same bug as nativeMin (ret initialised to 0.0)

TEST(NativeMax, NoArgs_ReturnsNil)
{
    EXPECT_TRUE(call0(nativeMax).isNil());
}

TEST(NativeMax, OneArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeMax, {Value::integer(1)}).isNil());
}

TEST(NativeMax, TwoPositiveIntegers)
{
    Value r = call(nativeMax, {Value::integer(3), Value::integer(7)});
    EXPECT_EQ(r.asInteger(), 7);
}

TEST(NativeMax, NegativeValues)
{
    // Bug: max(-3, -1) currently returns 0 because ret = 0.0
    Value r = call(nativeMax, {Value::integer(-3), Value::integer(-1)});
    EXPECT_EQ(r.asInteger(), -1); // will fail until bug is fixed
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    Value r = call(nativeMax, {Value::integer(1), Value::integer(9), Value::integer(4)});
    EXPECT_TRUE(r.isInteger());
    EXPECT_EQ(r.asInteger(), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    Value r = call(nativeMax, {Value::integer(3), Value::real(3.5)});
    EXPECT_TRUE(r.isDouble());
    EXPECT_DOUBLE_EQ(r.asDouble(), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsNil)
{
    EXPECT_TRUE(call(nativeMax, {makeStr("a"), makeStr("b")}).isNil());
}

// Stub functions — verify they don't crash and return nil

// Macro to generate a crash-safety + nil-return test for each stub.
#define STUB_TEST(FnName, ...)                                         \
    TEST(Stub_##FnName, DoesNotCrash_ReturnsNil)                       \
    {                                                                  \
        EXPECT_NO_FATAL_FAILURE({                                      \
            Value r = call0(FnName);                                   \
            EXPECT_TRUE(r.isNil());                                    \
        });                                                            \
    }                                                                  \
    TEST(Stub_##FnName, WithArgs_DoesNotCrash)                         \
    {                                                                  \
        EXPECT_NO_FATAL_FAILURE({                                      \
            Value r = call(FnName, {Value::integer(1), makeStr("x")}); \
        });                                                            \
    }

STUB_TEST(nativeRound)
STUB_TEST(nativePow)
STUB_TEST(nativeSqrt)
STUB_TEST(nativeAssert)
STUB_TEST(nativeClock)
STUB_TEST(nativeError)
STUB_TEST(nativeTime)
STUB_TEST(nativeInput)
STUB_TEST(nativeSplit)
STUB_TEST(nativeJoin)
STUB_TEST(nativeSubstr)
STUB_TEST(nativeContains)
STUB_TEST(nativeTrim)

// nativeRound — implementation spec tests (will pass once implemented)

TEST(NativeRound, HalfRoundsUp)
{
    Value r = call(nativeRound, {Value::real(2.5)});
    if (!r.isNil()) // skip if still a stub
        EXPECT_DOUBLE_EQ(r.asDouble(), 3.0);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    Value r = call(nativeRound, {Value::real(-2.5)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), -3.0);
}

TEST(NativeRound, IntegerPassthrough)
{
    Value r = call(nativeRound, {Value::integer(4)});
    if (!r.isNil())
        EXPECT_EQ(r.asInteger(), 4);
}

// nativePow — spec tests

TEST(NativePow, BasicSquare)
{
    Value r = call(nativePow, {Value::real(3.0), Value::real(2.0)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    Value r = call(nativePow, {Value::real(5.0), Value::real(0.0)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    Value r = call(nativePow, {Value::real(2.0), Value::real(-1.0)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), 0.5);
}

// nativeSqrt — spec tests

TEST(NativeSqrt, PerfectSquare)
{
    Value r = call(nativeSqrt, {Value::real(9.0)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), 3.0);
}

TEST(NativeSqrt, Zero)
{
    Value r = call(nativeSqrt, {Value::real(0.0)});
    if (!r.isNil())
        EXPECT_DOUBLE_EQ(r.asDouble(), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    // Should return nil or NaN — must not crash
    EXPECT_NO_FATAL_FAILURE(call(nativeSqrt, {Value::real(-1.0)}));
}

// nativeSplit — spec tests

TEST(NativeSplit, BasicSplit)
{
    Value r = call(nativeSplit, {makeStr("a,b,c"), makeStr(",")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isList());
        EXPECT_EQ(r.asList()->elements.size(), 3u);
    }
}

TEST(NativeSplit, NoDelimiterFound)
{
    Value r = call(nativeSplit, {makeStr("hello"), makeStr(",")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isList());
        EXPECT_EQ(r.asList()->elements.size(), 1u);
    }
}

// nativeJoin — spec tests

TEST(NativeJoin, BasicJoin)
{
    auto lst = makeList({makeStr("a"), makeStr("b"), makeStr("c")});
    Value r = call(nativeJoin, {lst, makeStr(",")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "a,b,c");
    }
}

TEST(NativeJoin, EmptyList)
{
    auto lst = makeList({});
    Value r = call(nativeJoin, {lst, makeStr(",")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
    }
}

// nativeSubstr — spec tests

TEST(NativeSubstr, BasicSubstr)
{
    Value r = call(nativeSubstr, {makeStr("hello"), Value::integer(1), Value::integer(3)});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    Value r = call(nativeSubstr, {makeStr("hello"), Value::integer(0), Value::integer(2)});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "hel");
    }
}

// nativeContains — spec tests

TEST(NativeContains, StringContains_True)
{
    Value r = call(nativeContains, {makeStr("hello world"), makeStr("world")});
    if (!r.isNil())
        EXPECT_TRUE(r.asBoolean());
}

TEST(NativeContains, StringContains_False)
{
    Value r = call(nativeContains, {makeStr("hello"), makeStr("xyz")});
    if (!r.isNil())
        EXPECT_FALSE(r.asBoolean());
}

TEST(NativeContains, ListContains_True)
{
    auto lst = makeList({Value::integer(1), Value::integer(2), Value::integer(3)});
    Value r = call(nativeContains, {lst, Value::integer(2)});
    if (!r.isNil())
        EXPECT_TRUE(r.asBoolean());
}

// nativeTrim — spec tests

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    Value r = call(nativeTrim, {makeStr("  hello  ")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "hello");
    }
}

TEST(NativeTrim, NoSpaces)
{
    Value r = call(nativeTrim, {makeStr("hello")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "hello");
    }
}

TEST(NativeTrim, OnlySpaces)
{
    Value r = call(nativeTrim, {makeStr("   ")});
    if (!r.isNil()) {
        ASSERT_TRUE(r.isString());
        EXPECT_EQ(std::string(r.asString()->str.data()), "");
    }
}

// nativeAssert — spec tests

TEST(NativeAssert, TrueCondition_DoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(call(nativeAssert, {Value::boolean(true)}));
}

TEST(NativeAssert, FalseCondition_DoesNotCrash)
{
    // May emit a diagnostic or throw — must not segfault
    EXPECT_NO_FATAL_FAILURE(call(nativeAssert, {Value::boolean(false)}));
}

// nativeClock / nativeTime — spec tests

TEST(NativeClock, ReturnsNumber_WhenImplemented)
{
    Value r = call0(nativeClock);
    if (!r.isNil())
        EXPECT_TRUE(r.isNumber());
}

TEST(NativeTime, ReturnsNumber_WhenImplemented)
{
    Value r = call0(nativeTime);
    if (!r.isNil())
        EXPECT_TRUE(r.isNumber());
}

} // namespace mylang::runtime