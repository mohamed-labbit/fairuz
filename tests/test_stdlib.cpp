#include "../include/stdlib.hpp"

#include <gtest/gtest.h>

using namespace mylang;
using namespace mylang::runtime;

static Value makeStr(char const* s) { return MAKE_STRING(StringRef(s)); }

static Value makeList(std::initializer_list<Value> elems = { })
{
    Value v = MAKE_LIST();
    for (auto const& e : elems)
        AS_LIST(v)->elements.push(e);
    return v;
}

static Value call(Value (*fn)(int, Value*), std::initializer_list<Value> args)
{
    std::vector<Value> v(args);
    return fn(static_cast<int>(v.size()), v.empty() ? nullptr : v.data());
}

static Value call0(Value (*fn)(int, Value*)) { return fn(0, nullptr); }

TEST(NativeLen, NoArgs) { EXPECT_TRUE(IS_NIL(call0(nativeLen))); }

TEST(NativeLen, NullArgv) { EXPECT_TRUE(IS_NIL(nativeLen(1, nullptr))); }

TEST(NativeLen, EmptyString)
{
    auto s = makeStr("");
    EXPECT_EQ(AS_INTEGER(call(nativeLen, { s })), 0);
}

TEST(NativeLen, NonEmptyString)
{
    auto s = makeStr("hello");
    EXPECT_EQ(AS_INTEGER(call(nativeLen, { s })), 5);
}

TEST(NativeLen, UnicodeString)
{
    auto s = makeStr("abc");
    EXPECT_EQ(AS_INTEGER(call(nativeLen, { s })), 3);
}

TEST(NativeLen, EmptyList)
{
    auto lst = makeList({ });
    EXPECT_EQ(AS_INTEGER(call(nativeLen, { lst })), 0);
}

TEST(NativeLen, NonEmptyList)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2), MAKE_INTEGER(3) });
    EXPECT_EQ(AS_INTEGER(call(nativeLen, { lst })), 3);
}

TEST(NativeLen, MultipleArgs_ReturnsNil)
{
    auto s = makeStr("hi");
    EXPECT_TRUE(IS_NIL(call(nativeLen, { s, s })));
}

TEST(NativeLen, IntegerArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeLen, { MAKE_INTEGER(42) }))); }

TEST(NativeLen, BoolArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeLen, { MAKE_BOOL(true) }))); }

TEST(NativeLen, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeLen, { NIL_VAL }))); }

TEST(NativePrint, NoArgs_PrintsNewline) { EXPECT_TRUE(IS_NIL(call0(nativePrint))); }

TEST(NativePrint, StringArg)
{
    auto s = makeStr("hello world");
    EXPECT_TRUE(IS_NIL(call(nativePrint, { s })));
}

TEST(NativePrint, IntegerArg) { EXPECT_TRUE(IS_NIL(call(nativePrint, { MAKE_INTEGER(42) }))); }

TEST(NativePrint, FloatArg) { EXPECT_TRUE(IS_NIL(call(nativePrint, { MAKE_REAL(3.14) }))); }

TEST(NativePrint, BoolArg_True) { EXPECT_TRUE(IS_NIL(call(nativePrint, { MAKE_BOOL(true) }))); }

TEST(NativePrint, BoolArg_False) { EXPECT_TRUE(IS_NIL(call(nativePrint, { MAKE_BOOL(false) }))); }

TEST(NativePrint, NilArg) { EXPECT_TRUE(IS_NIL(call(nativePrint, { NIL_VAL }))); }

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    auto s = makeStr("a");
    EXPECT_TRUE(IS_NIL(call(nativePrint, { s, s })));
}

TEST(NativeStr, NoArgs_ReturnsEmpty) { EXPECT_TRUE(IS_STRING(call0(nativeStr))); }

TEST(NativeStr, NullArgv_ReturnsNil) { EXPECT_TRUE(AS_STRING(nativeStr(1, nullptr))->str.empty()); }

TEST(NativeStr, Integer)
{
    Value r = call(nativeStr, { MAKE_INTEGER(42) });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    Value r = call(nativeStr, { MAKE_INTEGER(-7) });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    Value r = call(nativeStr, { MAKE_INTEGER(0) });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    Value r = call(nativeStr, { MAKE_BOOL(true) });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "true");
}

TEST(NativeStr, BoolFalse)
{
    Value r = call(nativeStr, { MAKE_BOOL(false) });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "false");
}

TEST(NativeStr, Float)
{
    Value r = call(nativeStr, { MAKE_REAL(1.5) });
    ASSERT_TRUE(IS_STRING(r));
    // std::to_string(1.5) == "1.500000" — just verify it parses back
    double parsed = std::stod(AS_STRING(r)->str.data());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    Value s = makeStr("hello");
    Value r = call(nativeStr, { s });
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
}

TEST(NativeStr, NilArg_ReturnsNil) { EXPECT_NO_FATAL_FAILURE(call(nativeStr, { NIL_VAL })); }

TEST(NativeStr, TwoArgs_ReturnsNil)
{
    auto s = makeStr("x");
    EXPECT_TRUE(IS_NIL(call(nativeStr, { s, s })));
}

TEST(NativeBool, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeBool))); }

TEST(NativeBool, TrueBoolean)
{
    Value r = call(nativeBool, { MAKE_BOOL(true) });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeBool, FalseBoolean)
{
    Value r = call(nativeBool, { MAKE_BOOL(false) });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    Value r = call(nativeBool, { MAKE_INTEGER(1) });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    Value r = call(nativeBool, { MAKE_INTEGER(0) });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NilArg)
{
    Value r = call(nativeBool, { NIL_VAL });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    Value r = call(nativeBool, { makeStr("hi") });
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeInt, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeInt))); }

TEST(NativeInt, IntegerPassthrough)
{
    Value r = call(nativeInt, { MAKE_INTEGER(7) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 7);
}

TEST(NativeInt, FloatTruncates)
{
    Value r = call(nativeInt, { MAKE_REAL(3.9) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 3);
}

TEST(NativeInt, NegativeFloat)
{
    Value r = call(nativeInt, { MAKE_REAL(-2.7) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), -2);
}

TEST(NativeInt, StringArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeInt, { makeStr("42") }))); }

TEST(NativeInt, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeInt, { NIL_VAL }))); }

TEST(NativeInt, BoolArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeInt, { MAKE_BOOL(true) }))); }

TEST(NativeFloat, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeFloat))); }

TEST(NativeFloat, IntegerToFloat)
{
    Value r = call(nativeFloat, { MAKE_INTEGER(3) });
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    Value r = call(nativeFloat, { MAKE_REAL(2.5) });
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    Value r = call(nativeFloat, { MAKE_INTEGER(-10) });
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -10.0);
}

TEST(NativeFloat, StringArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeFloat, { makeStr("3.14") }))); }

TEST(NativeFloat, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeFloat, { NIL_VAL }))); }

TEST(NativeType, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeType))); }

TEST(NativeType, ReturnsInteger)
{
    EXPECT_TRUE(IS_INTEGER(call(nativeType, { MAKE_INTEGER(0) })));
    EXPECT_TRUE(IS_INTEGER(call(nativeType, { MAKE_REAL(0.0) })));
    EXPECT_TRUE(IS_INTEGER(call(nativeType, { MAKE_BOOL(false) })));
    EXPECT_TRUE(IS_INTEGER(call(nativeType, { NIL_VAL })));
    EXPECT_TRUE(IS_INTEGER(call(nativeType, { makeStr("x") })));
}

TEST(NativeType, DifferentTypesHaveDifferentTags)
{
    int64_t int_tag = AS_INTEGER(call(nativeType, { MAKE_INTEGER(0) }));
    int64_t flt_tag = AS_INTEGER(call(nativeType, { MAKE_REAL(0.0) }));
    int64_t bool_tag = AS_INTEGER(call(nativeType, { MAKE_BOOL(false) }));
    int64_t nil_tag = AS_INTEGER(call(nativeType, { NIL_VAL }));
    int64_t str_tag = AS_INTEGER(call(nativeType, { makeStr("x") }));

    EXPECT_NE(int_tag, flt_tag);
    EXPECT_NE(int_tag, nil_tag);
    EXPECT_NE(str_tag, nil_tag);
    EXPECT_NE(bool_tag, nil_tag);
}

TEST(NativeAppend, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeAppend))); }

TEST(NativeAppend, OneArg_ReturnsNil)
{
    auto lst = makeList();
    EXPECT_TRUE(IS_NIL(call(nativeAppend, { lst })));
}

TEST(NativeAppend, AppendSingleElement)
{
    auto lst = makeList();
    call(nativeAppend, { lst, MAKE_INTEGER(99) });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 1u);
    EXPECT_EQ(AS_INTEGER(AS_LIST(lst)->elements[0]), 99);
}

TEST(NativeAppend, AppendMultipleElements)
{
    auto lst = makeList();
    call(nativeAppend, { lst, MAKE_INTEGER(1), MAKE_INTEGER(2), MAKE_INTEGER(3) });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 3u);
}

TEST(NativeAppend, AppendToNonEmptyList)
{
    auto lst = makeList({ MAKE_INTEGER(0) });
    call(nativeAppend, { lst, MAKE_INTEGER(1) });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 2u);
}

TEST(NativeAppend, AppendDifferentTypes)
{
    auto lst = makeList();
    call(nativeAppend, { lst, MAKE_INTEGER(1), MAKE_REAL(2.0), makeStr("x"), MAKE_BOOL(true) });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 4u);
}

TEST(NativeAppend, NonListFirstArg_ReturnsNil)
{
    Value x = MAKE_INTEGER(5);
    EXPECT_TRUE(IS_NIL(call(nativeAppend, { x, MAKE_INTEGER(1) })));
}

TEST(NativeAppend, ReturnValueIsNil)
{
    auto lst = makeList();
    EXPECT_TRUE(IS_NIL(call(nativeAppend, { lst, MAKE_INTEGER(1) })));
}

TEST(NativePop, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativePop))); }

TEST(NativePop, RemovesLastElement)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2) });
    call(nativeAppend, { lst, MAKE_INTEGER(3) });
    call(nativePop, { lst });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 2u);
}

TEST(NativePop, SingleElementList)
{
    auto lst = makeList({ MAKE_INTEGER(42) });
    call(nativePop, { lst });
    EXPECT_EQ(AS_LIST(lst)->elements.size(), 0u);
}

TEST(NativePop, ReturnValueIsNil)
{
    auto lst = makeList({ MAKE_INTEGER(1) });
    EXPECT_TRUE(IS_NIL(call(nativePop, { lst })));
}

TEST(NativePop, NonListArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativePop, { MAKE_INTEGER(5) }))); }

TEST(NativePop, NullArgv_ReturnsNil) { EXPECT_TRUE(IS_NIL(nativePop(1, nullptr))); }

TEST(NativeSlice, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeSlice))); }

TEST(NativeSlice, OneArg_ReturnsNil)
{
    auto lst = makeList({ MAKE_INTEGER(1) });
    EXPECT_TRUE(IS_NIL(call(nativeSlice, { lst })));
}

TEST(NativeSlice, TwoArgs_FromIndex)
{
    auto lst = makeList({ MAKE_INTEGER(10), MAKE_INTEGER(20), MAKE_INTEGER(30) });
    Value r = call(nativeSlice, { lst, MAKE_INTEGER(1) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 2u);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[0]), 20);
}

TEST(NativeSlice, ThreeArgs_SubRange)
{
    auto lst = makeList({ MAKE_INTEGER(10), MAKE_INTEGER(20), MAKE_INTEGER(30), MAKE_INTEGER(40) });
    Value r = call(nativeSlice, { lst, MAKE_INTEGER(1), MAKE_INTEGER(2) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 2u);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[0]), 20);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[1]), 30);
}

TEST(NativeSlice, WholeList)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2), MAKE_INTEGER(3) });
    Value r = call(nativeSlice, { lst, MAKE_INTEGER(0), MAKE_INTEGER(2) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 3u);
}

TEST(NativeSlice, SingleElement)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2), MAKE_INTEGER(3) });
    Value r = call(nativeSlice, { lst, MAKE_INTEGER(1), MAKE_INTEGER(1) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 1u);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[0]), 2);
}

TEST(NativeSlice, ReturnIsNewList_NotAlias)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2) });
    Value r = call(nativeSlice, { lst, MAKE_INTEGER(0), MAKE_INTEGER(0) });
    call(nativeAppend, { lst, MAKE_INTEGER(99) });
    EXPECT_EQ(AS_LIST(r)->elements.size(), 1u);
}

TEST(NativeList, NoArgs_ReturnsEmpty)
{
    Value r = call0(nativeList);
    EXPECT_TRUE(IS_LIST(r) && AS_LIST(r)->elements.empty());
}

TEST(NativeList, OneArg)
{
    Value v = MAKE_INTEGER(1);
    Value r = call(nativeList, { v });
    EXPECT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements[0], v);
}

TEST(NativeList, TwoArgs_CreatesList)
{
    Value r = call(nativeList, { MAKE_INTEGER(1), MAKE_INTEGER(2) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 2u);
}

TEST(NativeList, MixedTypes)
{
    Value r = call(nativeList, { MAKE_INTEGER(1), makeStr("x"), MAKE_BOOL(false) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 3u);
}

TEST(NativeList, PreservesOrder)
{
    Value r = call(nativeList, { MAKE_INTEGER(10), MAKE_INTEGER(20), MAKE_INTEGER(30) });
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[0]), 10);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[1]), 20);
    EXPECT_EQ(AS_INTEGER(AS_LIST(r)->elements[2]), 30);
}

TEST(NativeFloor, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeFloor))); }

TEST(NativeFloor, IntegerPassthrough)
{
    Value r = call(nativeFloor, { MAKE_INTEGER(5) });
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    Value r = call(nativeFloor, { MAKE_REAL(3.7) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeFloor, NegativeFloat)
{
    Value r = call(nativeFloor, { MAKE_REAL(-2.3) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -3.0);
}

TEST(NativeFloor, ExactFloat)
{
    Value r = call(nativeFloor, { MAKE_REAL(4.0) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeFloor, StringArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeFloor, { makeStr("3.5") }))); }

TEST(NativeFloor, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeFloor, { NIL_VAL }))); }

TEST(NativeFloor, TwoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeFloor, { MAKE_REAL(1.5), MAKE_REAL(2.5) }))); }

TEST(NativeCeil, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeCeil))); }

TEST(NativeCeil, IntegerPassthrough)
{
    Value r = call(nativeCeil, { MAKE_INTEGER(5) });
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    Value r = call(nativeCeil, { MAKE_REAL(3.2) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, NegativeFloat)
{
    Value r = call(nativeCeil, { MAKE_REAL(-2.7) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -2.0);
}

TEST(NativeCeil, ExactFloat)
{
    Value r = call(nativeCeil, { MAKE_REAL(4.0) });
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, StringArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeCeil, { makeStr("3.5") }))); }

TEST(NativeCeil, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeCeil, { NIL_VAL }))); }

TEST(NativeAbs, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeAbs))); }

TEST(NativeAbs, PositiveInteger)
{
    Value r = call(nativeAbs, { MAKE_INTEGER(5) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    Value r = call(nativeAbs, { MAKE_INTEGER(-5) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    Value r = call(nativeAbs, { MAKE_INTEGER(0) });
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    Value r = call(nativeAbs, { MAKE_REAL(3.5) });
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    Value r = call(nativeAbs, { MAKE_REAL(-3.5) });
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, StringArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeAbs, { makeStr("-3") }))); }

TEST(NativeAbs, NilArg_ReturnsNil) { EXPECT_TRUE(IS_NIL(call(nativeAbs, { NIL_VAL }))); }

TEST(NativeMin, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeMin))); }

TEST(NativeMin, OneArg_ReturnsArg)
{
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = MAKE_INTEGER(static_cast<int64_t>(rand()));
    Value result = nativeMin(1, &n);
    EXPECT_EQ(AS_INTEGER(result), AS_INTEGER(n));
}

TEST(NativeMin, TwoPositiveIntegers)
{
    Value r = call(nativeMin, { MAKE_INTEGER(3), MAKE_INTEGER(7) });
    EXPECT_EQ(AS_INTEGER(r), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    Value r = call(nativeMin, { MAKE_INTEGER(5), MAKE_INTEGER(2), MAKE_INTEGER(8) });
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    Value r = call(nativeMin, { MAKE_INTEGER(3), MAKE_REAL(1.5) });
    EXPECT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    Value r = call(nativeMin, { MAKE_INTEGER(-1), MAKE_INTEGER(-5), MAKE_INTEGER(-2) });
    EXPECT_EQ(AS_INTEGER(r), -5);
}

TEST(NativeMin, StringFirstArg_ReturnsNil) { EXPECT_EQ(AS_STRING(call(nativeMin, { makeStr("a"), makeStr("b") }))->str, MAKE_OBJ_STRING("a")->str); }

TEST(NativeMax, NoArgs_ReturnsNil) { EXPECT_TRUE(IS_NIL(call0(nativeMax))); }

TEST(NativeMax, OneArg_ReturnsArg)
{
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = MAKE_INTEGER(static_cast<int64_t>(rand()));
    Value result = nativeMax(1, &n);
    EXPECT_EQ(AS_INTEGER(result), AS_INTEGER(n));
}

TEST(NativeMax, TwoPositiveIntegers)
{
    Value r = call(nativeMax, { MAKE_INTEGER(3), MAKE_INTEGER(7) });
    EXPECT_EQ(AS_INTEGER(r), 7);
}

TEST(NativeMax, NegativeValues)
{
    Value r = call(nativeMax, { MAKE_INTEGER(-3), MAKE_INTEGER(-1) });
    EXPECT_EQ(AS_INTEGER(r), -1);
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    Value r = call(nativeMax, { MAKE_INTEGER(1), MAKE_INTEGER(9), MAKE_INTEGER(4) });
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    Value r = call(nativeMax, { MAKE_INTEGER(3), MAKE_REAL(3.5) });
    EXPECT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsArg) { EXPECT_EQ(AS_STRING(call(nativeMax, { makeStr("a"), makeStr("b") }))->str, MAKE_OBJ_STRING("b")->str); }

#define STUB_TEST(FnName, ...)                                                                   \
    TEST(Stub_##FnName, DoesNotCrash_ReturnsNil)                                                 \
    {                                                                                            \
        EXPECT_NO_FATAL_FAILURE({                                                                \
            Value r = call0(FnName);                                                             \
            EXPECT_TRUE(IS_NIL(r));                                                              \
        });                                                                                      \
    }                                                                                            \
    TEST(Stub_##FnName, WithArgs_DoesNotCrash)                                                   \
    {                                                                                            \
        EXPECT_NO_FATAL_FAILURE({ Value r = call(FnName, { MAKE_INTEGER(1), makeStr("x") }); }); \
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

TEST(NativeRound, HalfRoundsUp)
{
    Value r = call(nativeRound, { MAKE_REAL(2.5) });
    if (!IS_NIL(r)) // skip if still a stub
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    Value r = call(nativeRound, { MAKE_REAL(-2.5) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -3.0);
}

TEST(NativeRound, IntegerPassthrough)
{
    Value r = call(nativeRound, { MAKE_INTEGER(4) });
    if (!IS_NIL(r))
        EXPECT_EQ(AS_INTEGER(r), 4);
}

TEST(NativePow, BasicSquare)
{
    Value r = call(nativePow, { MAKE_REAL(3.0), MAKE_REAL(2.0) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    Value r = call(nativePow, { MAKE_REAL(5.0), MAKE_REAL(0.0) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    Value r = call(nativePow, { MAKE_REAL(2.0), MAKE_REAL(-1.0) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 0.5);
}

TEST(NativeSqrt, PerfectSquare)
{
    Value r = call(nativeSqrt, { MAKE_REAL(9.0) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeSqrt, Zero)
{
    Value r = call(nativeSqrt, { MAKE_REAL(0.0) });
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior) { EXPECT_NO_FATAL_FAILURE(call(nativeSqrt, { MAKE_REAL(-1.0) })); }

TEST(NativeSplit, BasicSplit)
{
    Value r = call(nativeSplit, { makeStr("a,b,c"), makeStr(",") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_LIST(r));
        EXPECT_EQ(AS_LIST(r)->elements.size(), 3u);
    }
}

TEST(NativeSplit, NoDelimiterFound)
{
    Value r = call(nativeSplit, { makeStr("hello"), makeStr(",") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_LIST(r));
        EXPECT_EQ(AS_LIST(r)->elements.size(), 1u);
    }
}

TEST(NativeJoin, BasicJoin)
{
    auto lst = makeList({ makeStr("a"), makeStr("b"), makeStr("c") });
    Value r = call(nativeJoin, { lst, makeStr(",") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "a,b,c");
    }
}

TEST(NativeJoin, EmptyList)
{
    auto lst = makeList({ });
    Value r = call(nativeJoin, { lst, makeStr(",") });
    if (!IS_NIL(r))
        ASSERT_TRUE(IS_STRING(r));
}

TEST(NativeSubstr, BasicSubstr)
{
    // substr is exlusive
    Value r = call(nativeSubstr, { makeStr("hello"), MAKE_INTEGER(1), MAKE_INTEGER(4) });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    // substr is exclusive
    Value r = call(nativeSubstr, { makeStr("hello"), MAKE_INTEGER(0), MAKE_INTEGER(3) });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hel");
    }
}

TEST(NativeContains, StringContains_True)
{
    Value r = call(nativeContains, { makeStr("hello world"), makeStr("world") });
    if (!IS_NIL(r))
        EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeContains, StringContains_False)
{
    Value r = call(nativeContains, { makeStr("hello"), makeStr("xyz") });
    if (!IS_NIL(r))
        EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeContains, ListContains_True)
{
    auto lst = makeList({ MAKE_INTEGER(1), MAKE_INTEGER(2), MAKE_INTEGER(3) });
    Value r = call(nativeContains, { lst, MAKE_INTEGER(2) });
    if (!IS_NIL(r))
        EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    Value r = call(nativeTrim, { makeStr("  hello  ") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
    }
}

TEST(NativeTrim, NoSpaces)
{
    Value r = call(nativeTrim, { makeStr("hello") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
    }
}

TEST(NativeTrim, OnlySpaces)
{
    Value r = call(nativeTrim, { makeStr("   ") });
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "");
    }
}

TEST(NativeAssert, TrueCondition_DoesNotCrash) { EXPECT_NO_FATAL_FAILURE(call(nativeAssert, { MAKE_BOOL(true) })); }

TEST(NativeAssert, FalseCondition_DoesNotCrash) { EXPECT_NO_FATAL_FAILURE(call(nativeAssert, { MAKE_BOOL(false) })); }

TEST(NativeClock, ReturnsNumber_WhenImplemented)
{
    Value r = call0(nativeClock);
    if (!IS_NIL(r))
        EXPECT_TRUE(IS_INTEGER(r));
}

TEST(NativeTime, ReturnsNumber_WhenImplemented)
{
    Value r = call0(nativeTime);
    if (!IS_NIL(r))
        EXPECT_TRUE(IS_INTEGER(r));
}
