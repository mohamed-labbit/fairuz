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
    // Values inside the active payload range stay small in either representation.
    for (i64 n : { Value::int_min(), Value::int_min() + 1, i64 { 0 }, Value::int_max() - 1, Value::int_max() }) {
        Value v = Value::from_int(n, gc);
        EXPECT_FALSE(v.is_big_int());
        EXPECT_EQ(v.as_int(), n);
    }
    // Parse boundary values from text so the 64-bit cases cannot overflow in C++.
#if FA_USE_NANBOX
    StringRef a = "-140737488355330"; // INT48_MIN - 2
    StringRef b = "-140737488355329"; // INT48_MIN - 1
    StringRef c = "140737488355328";  // INT48_MAX + 1
    StringRef d = "140737488355329";  // INT48_MAX + 2
#else
    StringRef a = "-9223372036854775810"; // INT64_MIN - 2
    StringRef b = "-9223372036854775809"; // INT64_MIN - 1
    StringRef c = "9223372036854775808";  // INT64_MAX + 1
    StringRef d = "9223372036854775809";  // INT64_MAX + 2
#endif
    for (StringRef n : { a, b, c, d }) {
        integer::Data parsed = integer::parse(n, /*base=*/10);
        integer::Value value = integer::finish(parsed, gc);
        ASSERT_TRUE(value.is_big_int());
        auto const& limbs = value.as_big_int()->limbs;
        ASSERT_EQ(limbs.size(), parsed.limbs.size());
        for (u32 i = 0, n = limbs.size(); i < n; ++i)
            EXPECT_EQ(limbs[i], parsed.limbs[i]);
        EXPECT_EQ(value.as_big_int()->sign, parsed.positive);
        EXPECT_EQ(integer::to_string(value).c_str(), n);
    }
    EXPECT_EQ(integer::to_string(integer::neg(Value::from_int(INT64_MIN, gc), gc)), "9223372036854775808");
    EXPECT_EQ(integer::to_string(integer::add(Value::from_int(INT64_MAX, gc), Value::from_int(1), gc)), "9223372036854775808");
    EXPECT_EQ(integer::to_string(integer::sub(Value::from_int(INT64_MIN, gc), Value::from_int(1), gc)), "-9223372036854775809");
    Value native_minimum = decimal("-9223372036854775808", gc);
    EXPECT_EQ(integer::to_string(native_minimum), "-9223372036854775808");
    EXPECT_EQ(native_minimum.as_int(), INT64_MIN);
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

    // Negating the payload minimum crosses the small-integer boundary in either representation.
    Value minimum = Value::from_int(Value::int_min(), gc);
    ASSERT_FALSE(minimum.is_big_int());
    Value quotient = integer::div(minimum, Value::from_int(-1), gc);
    EXPECT_TRUE(quotient.is_big_int());
    EXPECT_EQ(integer::to_string(quotient), wide_string(-static_cast<__int128>(Value::int_min())));
    EXPECT_EQ(integer::div(minimum, Value::from_int(-1), gc, true).as_int(), 0);

    // Keep the native overflow case covered even when the payload is only 48 bits.
    EXPECT_EQ(integer::to_string(integer::div(Value::from_int(INT64_MIN, gc), Value::from_int(-1), gc)), "9223372036854775808");
    EXPECT_EQ(integer::div(Value::from_int(INT64_MIN, gc), Value::from_int(-1), gc, true).as_int(), 0);

    i64 boundary_exponent = FA_USE_NANBOX ? 47 : 63;
    for (i64 exponent : { boundary_exponent - 1, boundary_exponent }) {
        Value power = integer::pow(Value::from_int(2), Value::from_int(exponent), gc);
        EXPECT_EQ(power.is_big_int(), exponent == boundary_exponent);
        EXPECT_EQ(integer::to_string(power), wide_string(static_cast<__int128>(1) << exponent));
    }
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
    EXPECT_FALSE(ValueEqual { }(f, a));
    EXPECT_TRUE(ValueEqual { }(b, f));
    EXPECT_TRUE(ValueEqual { }(f, b));
    EXPECT_FALSE(ValueEqual { }(Value::from_bool(true), Value::from_int(1)));
    EXPECT_FALSE(ValueEqual { }(Value::from_int(1), Value::from_bool(true)));
    EXPECT_EQ(ValueHash { }(b), ValueHash { }(f));
    DictType dict;
    dict[a] = Value::from_int(1);
    dict[b] = Value::from_int(2);
    EXPECT_EQ(dict.size(), 2);
    auto* found = dict.find_ptr(f);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->as_int(), 2);
    EXPECT_EQ(integer::compare_numbers(a, f), 1);
    EXPECT_EQ(integer::compare_numbers(Value::from_int(0), Value::from_real(-0.5)), 1);
    EXPECT_EQ(integer::compare_numbers(a, Value::from_real(std::numeric_limits<double>::infinity())), -1);
    EXPECT_EQ(integer::to_string(integer::from_double(0x1p100, gc)), "1267650600228229401496703205376");
    EXPECT_EQ(integer::from_double(-3.75, gc).as_int(), -3);
    EXPECT_EQ(integer::to_string(integer::finish(integer::parse("١٨٤٤٦٧٤٤٠٧٣٧٠٩٥٥١٦١٦", 10), gc)), "18446744073709551616");
}

TEST(IntegerContract, BigIntegersPreserveNumericHashAndConversions)
{
    GarbageCollector gc;
    Value a = decimal("1267650600228229401496703205376", gc); // 2^100
    Value b = decimal("1267650600228229401496703205376", gc);
    Value f = Value::from_real(0x1p100);
    ASSERT_TRUE(a.is_big_int());
    ASSERT_TRUE(b.is_big_int());
    EXPECT_TRUE(ValueEqual { }(a, b));
    EXPECT_TRUE(ValueEqual { }(a, f));
    EXPECT_EQ(ValueHash { }(a), ValueHash { }(b));
    EXPECT_EQ(ValueHash { }(a), ValueHash { }(f));
    EXPECT_DOUBLE_EQ(a.as_double_any(), 0x1p100);
    EXPECT_DOUBLE_EQ(integer::neg(a, gc).as_double_any(), -0x1p100);

    DictType dict;
    dict[a] = Value::from_int(1);
    dict[b] = Value::from_int(2);
    EXPECT_EQ(dict.size(), 1);
    auto* found = dict.find_ptr(f);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->as_int(), 2);
}

TEST(IntegerContract, NativeIntegerConversionChecksBigIntegerRange)
{
    GarbageCollector gc;
    // Native callers must see the value, even for deliberately boxed small integers.
    for (i64 n : { INT64_MIN, i64 { -1 }, i64 { 0 }, i64 { 1 }, INT64_MAX }) {
        Value boxed = Value::from_obj(&gc.make_obj_int(integer::from_i64(n))->obj);
        EXPECT_EQ(boxed.as_int(), n);
        EXPECT_EQ(ValueHash { }(boxed), ValueHash { }(Value::from_int(n, gc)));
    }
    EXPECT_THROW(decimal("9223372036854775808", gc).as_int(), diagnostic::DiagnosticAbort);
    EXPECT_THROW(decimal("-9223372036854775809", gc).as_int(), diagnostic::DiagnosticAbort);
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
