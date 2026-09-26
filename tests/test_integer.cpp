#include "fgc.hpp"
#include "finteger.hpp"
#include "fvm.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <random>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

Value decimal(char const* text, GarbageCollector& gc) { return integer::finish(integer::parse(StringRef(text), 10), gc); }
std::string wide_string(__int128 value)
{
    bool negative = value < 0;
    unsigned __int128 n = negative ? -static_cast<unsigned __int128>(value) : value;
    std::string out;
    do {
        out.push_back('0' + n % 10);
        n /= 10;
    } while (n);
    if (negative)
        out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

}

TEST(IntegerContract, PayloadBoundariesAndNativeMinimum)
{
    GarbageCollector gc;
    for (i64 n : { Value::int_min(), Value::int_min() + 1, i64 { 0 }, Value::int_max() - 1, Value::int_max() }) {
        Value v = Value::from_int(n, gc);
        EXPECT_FALSE(v.is_big_int());
        EXPECT_EQ(v.as_int(), n);
    }
    for (i64 n : { INT64_MIN, Value::int_min() - 1, Value::int_max() + 1, INT64_MAX }) {
        Value v = Value::from_int(n, gc);
        EXPECT_TRUE(v.is_big_int());
        EXPECT_EQ(v.as_int(), n);
        EXPECT_EQ(integer::to_string(v), std::to_string(n));
    }
    EXPECT_EQ(integer::to_string(integer::neg(Value::from_int(INT64_MIN, gc), gc)), "9223372036854775808");
    EXPECT_EQ(integer::to_string(integer::add(Value::from_int(INT64_MAX, gc), Value::from_int(1), gc)), "9223372036854775808");
    EXPECT_EQ(integer::to_string(integer::sub(Value::from_int(INT64_MIN, gc), Value::from_int(1), gc)), "-9223372036854775809");
}

TEST(IntegerContract, SignedArithmeticMatchesWideOracle)
{
    GarbageCollector gc;
    std::mt19937_64 random(20260925);
    for (int i = 0; i < 300; ++i) {
        i64 a = static_cast<i64>(random() >> 1), b = static_cast<i64>(random() >> 1);
        if (i % 2)
            a = -a;
        if (i % 3)
            b = -b;
        if (i % 4 == 0)
            a %= 1000;
        if (i % 4 == 1)
            b %= 1000;
        Value x = Value::from_int(a, gc), y = Value::from_int(b, gc);
        EXPECT_EQ(integer::to_string(integer::add(x, y, gc)), wide_string(static_cast<__int128>(a) + b));
        EXPECT_EQ(integer::to_string(integer::sub(x, y, gc)), wide_string(static_cast<__int128>(a) - b));
        EXPECT_EQ(integer::to_string(integer::mul(x, y, gc)), wide_string(static_cast<__int128>(a) * b));
        if (b)
            EXPECT_EQ(integer::to_string(integer::div(x, y, gc, true)), std::to_string(a % b));
        EXPECT_EQ(integer::compare(x, y), a < b ? -1 : a > b ? 1
                                                             : 0);
        EXPECT_EQ(integer::to_string(x), std::to_string(a));
        EXPECT_EQ(integer::to_string(y), std::to_string(b));
    }
}

TEST(IntegerContract, CarryBorrowCancellationAndImmutability)
{
    GarbageCollector gc;
    Value all = decimal("340282366920938463463374607431768211455", gc);
    Value one = Value::from_int(1);
    Value next = integer::add(all, one, gc);
    EXPECT_EQ(integer::to_string(next), "340282366920938463463374607431768211456");
    EXPECT_EQ(integer::to_string(integer::sub(next, one, gc)), integer::to_string(all));
    EXPECT_EQ(integer::to_string(integer::add(all, all, gc)), "680564733841876926926749214863536422910");
    EXPECT_EQ(integer::to_string(all), "340282366920938463463374607431768211455");
    Value zero = integer::sub(all, all, gc);
    EXPECT_FALSE(zero.is_big_int());
    EXPECT_FALSE(zero.is_truthy());
    Value small = integer::sub(next, all, gc);
    EXPECT_FALSE(small.is_big_int());
    EXPECT_EQ(small.as_int(), 1);
    Value normalized = integer::finish({ { 7, 0, 0 }, false }, gc);
    EXPECT_EQ(normalized.as_int(), -7);
    Value raw_zero = Value::from_obj(&gc.make_obj_int({ { 0, 0 }, false })->obj);
    EXPECT_FALSE(raw_zero.is_truthy());
    EXPECT_TRUE(raw_zero.as_big_int()->sign);
    EXPECT_TRUE(ValueEqual { }(raw_zero, zero));
}

TEST(IntegerContract, DivisionRemainderAndPower)
{
    GarbageCollector gc;
    Value a = decimal("123456789012345678901234567890", gc), b = decimal("98765432109876543210", gc);
    Value product = integer::mul(a, b, gc);
    EXPECT_EQ(integer::to_string(integer::div(product, b, gc)), integer::to_string(a));
    Value dividend = integer::add(product, Value::from_int(17), gc);
    EXPECT_EQ(integer::div(dividend, b, gc, true).as_int(), 17);
    EXPECT_EQ(integer::div(integer::neg(dividend, gc), b, gc, true).as_int(), -17);
    EXPECT_DOUBLE_EQ(integer::div(Value::from_int(-7), Value::from_int(2), gc).as_double(), -3.5);
    EXPECT_EQ(integer::to_string(integer::div(Value::from_int(INT64_MIN, gc), Value::from_int(-1), gc)), "9223372036854775808");
    EXPECT_EQ(integer::div(Value::from_int(INT64_MIN, gc), Value::from_int(-1), gc, true).as_int(), 0);
    EXPECT_EQ(integer::to_string(integer::pow(Value::from_int(2), Value::from_int(128), gc)), "340282366920938463463374607431768211456");
}

TEST(IntegerContract, ShiftWidthsAndSignedBitwise)
{
    GarbageCollector gc;
    for (i64 n : { 0, 31, 32, 47, 48, 63, 64, 65, 127, 128, 256 }) {
        Value count = Value::from_int(n), one = Value::from_int(1);
        Value power = integer::shift(one, count, true, gc);
        EXPECT_EQ(integer::shift(power, count, false, gc).as_int(), 1);
        EXPECT_EQ(integer::shift(integer::neg(power, gc), count, false, gc).as_int(), -1);
        Value lower = integer::sub(integer::neg(power, gc), one, gc);
        EXPECT_EQ(integer::shift(lower, count, false, gc).as_int(), -2);
        EXPECT_TRUE(ValueEqual { }(integer::bitwise(power, Value::from_int(-1), '&', gc), power));
        EXPECT_EQ(integer::bitwise(power, Value::from_int(-1), '|', gc).as_int(), -1);
    }
    Value huge = decimal("999999999999999999999999999999999", gc);
    EXPECT_EQ(integer::shift(Value::from_int(-9), huge, false, gc).as_int(), -1);
    EXPECT_EQ(integer::shift(Value::from_int(9), huge, false, gc).as_int(), 0);
    EXPECT_THROW(integer::shift(Value::from_int(1), Value::from_int(-1), true, gc), diagnostic::DiagnosticAbort);
    EXPECT_THROW(integer::shift(Value::from_int(1), huge, true, gc), diagnostic::DiagnosticAbort);
}

TEST(IntegerContract, ExactComparisonHashAndConversions)
{
    GarbageCollector gc;
    Value a = decimal("9007199254740993", gc), b = decimal("9007199254740992", gc), f = Value::from_real(9007199254740992.0);
    EXPECT_FALSE(ValueEqual { }(a, b));
    EXPECT_FALSE(ValueEqual { }(a, f));
    EXPECT_TRUE(ValueEqual { }(b, f));
    EXPECT_EQ(ValueHash { }(b), ValueHash { }(f));
    DictType dict;
    dict[a] = Value::from_int(1);
    dict[b] = Value::from_int(2);
    EXPECT_EQ(dict.size(), 2);
    EXPECT_EQ(dict.find_ptr(f)->as_int(), 2);
    EXPECT_EQ(integer::compare_numbers(a, f), 1);
    EXPECT_EQ(integer::compare_numbers(Value::from_int(0), Value::from_real(-0.5)), 1);
    EXPECT_EQ(integer::compare_numbers(a, Value::from_real(std::numeric_limits<double>::infinity())), -1);
    EXPECT_EQ(integer::to_string(integer::from_double(0x1p100, gc)), "1267650600228229401496703205376");
    EXPECT_EQ(integer::from_double(-3.75, gc).as_int(), -3);
    EXPECT_EQ(integer::to_string(integer::finish(integer::parse("١٨٤٤٦٧٤٤٠٧٣٧٠٩٥٥١٦١٦", 10), gc)), "18446744073709551616");
}

TEST(IntegerContract, NativeTextConversionPreservesLargeIntegers)
{
    VM vm;
    Value text = vm.m_gc.make_string("18446744073709551616000000000000000000");
    EXPECT_EQ(integer::to_string(vm.number_from_text(1, &text)), "18446744073709551616000000000000000000");
    text = vm.m_gc.make_string("-18446744073709551616000000000000000000");
    EXPECT_EQ(integer::to_string(vm.number_from_text(1, &text)), "-18446744073709551616000000000000000000");
    text = vm.m_gc.make_string("18446744073709551616000000000000000000x");
    EXPECT_TRUE(vm.number_from_text(1, &text).is_nil());
}
