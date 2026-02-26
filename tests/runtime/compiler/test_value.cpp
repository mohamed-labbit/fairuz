#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

#include "../../../include/runtime/compiler/value.hpp"
#include "../../../include/runtime/opcode/chunk.hpp"

using namespace mylang::runtime;

// NaN-boxing bit layout invariants

TEST(NanBox, NilRawBits)
{
    // NIL_VAL must equal QNAN | TAG_NIL (tag=1)
    EXPECT_EQ(Value::nil().raw, NANBOX_QNAN | TAG_NIL);
}

TEST(NanBox, FalseRawBits)
{
    EXPECT_EQ(Value::boolean(false).raw, NANBOX_QNAN | TAG_BOOL);
}

TEST(NanBox, TrueRawBits)
{
    // true encodes the bool bit at bit 2
    EXPECT_EQ(Value::boolean(true).raw, NANBOX_QNAN | TAG_BOOL | (UINT64_C(1) << 2));
}

TEST(NanBox, ZeroIntegerRawBits)
{
    // Integer 0: QNAN | TAG_INT(3) | payload(0 << 3)
    Value v = Value::integer(0);
    EXPECT_TRUE(v.isInt());
    EXPECT_FALSE(v.isDouble());
    EXPECT_FALSE(v.isNil());
    EXPECT_FALSE(v.isBool());
    EXPECT_FALSE(v.isObj());
}

TEST(NanBox, PureDoubleIsNotTagged)
{
    // A normal double must not have all of bits 51..62 set
    Value v = Value::real(1.0);
    EXPECT_TRUE(v.isDouble());
    EXPECT_FALSE(v.isInt());
    EXPECT_FALSE(v.isNil());
    EXPECT_FALSE(v.isBool());
    EXPECT_FALSE(v.isObj());
}

TEST(NanBox, ObjPointerTagging)
{
    // Object: SIGN_BIT | QNAN in upper bits, pointer in lower 48
    ObjList list;
    Value v = Value::object(&list);
    EXPECT_TRUE(v.isObj());
    EXPECT_FALSE(v.isDouble());
    EXPECT_FALSE(v.isInt());
    EXPECT_FALSE(v.isNil());
    EXPECT_FALSE(v.isBool());
    EXPECT_EQ(v.asObj(), static_cast<ObjHeader*>(&list));
}

// Type tag mutual exclusivity — no two tags should fire at once

TEST(NanBox, TypeTagsMutuallyExclusive_Nil)
{
    Value v = Value::nil();
    int count = v.isNil() + v.isBool() + v.isInt() + v.isDouble() + v.isObj();
    EXPECT_EQ(count, 1);
}

TEST(NanBox, TypeTagsMutuallyExclusive_Bool)
{
    for (bool b : { true, false }) {
        Value v = Value::boolean(b);
        int count = v.isNil() + v.isBool() + v.isInt() + v.isDouble() + v.isObj();
        EXPECT_EQ(count, 1) << "bool=" << b;
    }
}

TEST(NanBox, TypeTagsMutuallyExclusive_Int)
{
    for (int64_t i : { INT64_C(0), INT64_C(1), INT64_C(-1), INT64_C(100000) }) {
        Value v = Value::integer(i);
        int count = v.isNil() + v.isBool() + v.isInt() + v.isDouble() + v.isObj();
        EXPECT_EQ(count, 1) << "int=" << i;
    }
}

TEST(NanBox, TypeTagsMutuallyExclusive_Double)
{
    for (double d : { 0.0, 1.5, -3.14, 1e300 }) {
        Value v = Value::real(d);
        int count = v.isNil() + v.isBool() + v.isInt() + v.isDouble() + v.isObj();
        EXPECT_EQ(count, 1) << "double=" << d;
    }
}

TEST(NanBox, TypeTagsMutuallyExclusive_Obj)
{
    ObjList list;
    Value v = Value::object(&list);
    int count = v.isNil() + v.isBool() + v.isInt() + v.isDouble() + v.isObj();
    EXPECT_EQ(count, 1);
}

// Integer round-trip — all boundary values

TEST(ValueInt, Zero)
{
    Value v = Value::integer(0);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), 0);
}

TEST(ValueInt, PositiveOne)
{
    Value v = Value::integer(1);
    EXPECT_EQ(v.asInt(), 1);
}

TEST(ValueInt, NegativeOne)
{
    Value v = Value::integer(-1);
    EXPECT_EQ(v.asInt(), -1);
}

TEST(ValueInt, LargePositive)
{
    int64_t n = (INT64_C(1) << 40);
    Value v = Value::integer(n);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), n);
}

TEST(ValueInt, LargeNegative)
{
    int64_t n = -(INT64_C(1) << 40);
    Value v = Value::integer(n);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), n);
}

TEST(ValueInt, Max48BitPositive)
{
    // 2^47 - 1 is the maximum value that fits in the int48 encoding
    int64_t n = (INT64_C(1) << 47) - 1;
    Value v = Value::integer(n);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), n);
}

TEST(ValueInt, Min48BitNegative)
{
    // -(2^47) is the minimum value that fits in the int48 encoding
    int64_t n = -(INT64_C(1) << 47);
    Value v = Value::integer(n);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(v.asInt(), n);
}

TEST(ValueInt, OverflowFallsBackToDouble)
{
    // Values outside ±2^47 must be stored as doubles
    int64_t n = INT64_C(1) << 48;
    Value v = Value::integer(n);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), static_cast<double>(n));
}

TEST(ValueInt, SignExtensionCorrect)
{
    // -1 stored as int48: all 48 bits set. asInt() must sign-extend correctly.
    Value v = Value::integer(-1);
    EXPECT_EQ(v.asInt(), -1LL);
}

// Double round-trip

TEST(ValueDouble, PositiveFinite)
{
    Value v = Value::real(3.14);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), 3.14);
}

TEST(ValueDouble, NegativeFinite)
{
    Value v = Value::real(-2.718);
    EXPECT_DOUBLE_EQ(v.asDouble(), -2.718);
}

TEST(ValueDouble, Zero)
{
    Value v = Value::real(0.0);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), 0.0);
}

TEST(ValueDouble, NegativeZero)
{
    // -0.0 should survive as a double (distinct bit pattern from +0.0)
    Value v = Value::real(-0.0);
    EXPECT_TRUE(v.isDouble());
    // -0.0 == 0.0 numerically, but the bit pattern differs
    double d = v.asDouble();
    EXPECT_DOUBLE_EQ(d, 0.0);
}

TEST(ValueDouble, PositiveInfinity)
{
    double inf = std::numeric_limits<double>::infinity();
    Value v = Value::real(inf);
    EXPECT_TRUE(v.isDouble());
    EXPECT_TRUE(std::isinf(v.asDouble()));
    EXPECT_GT(v.asDouble(), 0.0);
}

TEST(ValueDouble, NegativeInfinity)
{
    double inf = -std::numeric_limits<double>::infinity();
    Value v = Value::real(inf);
    EXPECT_TRUE(v.isDouble());
    EXPECT_TRUE(std::isinf(v.asDouble()));
    EXPECT_LT(v.asDouble(), 0.0);
}

TEST(ValueDouble, NaNNormalized)
{
    // User NaN gets normalised — it must still be a double and still be NaN
    Value v = Value::real(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(v.isDouble());
    EXPECT_TRUE(std::isnan(v.asDouble()));
}

TEST(ValueDouble, MaxDouble)
{
    double m = std::numeric_limits<double>::max();
    Value v = Value::real(m);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), m);
}

TEST(ValueDouble, SmallestPositive)
{
    double s = std::numeric_limits<double>::min();
    Value v = Value::real(s);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), s);
}

// as_number_double — unified numeric accessor

TEST(ValueDouble, AsNumberDoubleFromInt)
{
    Value v = Value::integer(42);
    EXPECT_DOUBLE_EQ(v.as_number_double(), 42.0);
}

TEST(ValueDouble, AsNumberDoubleFromDouble)
{
    Value v = Value::real(1.5);
    EXPECT_DOUBLE_EQ(v.as_number_double(), 1.5);
}

// Boolean semantics

TEST(ValueBool, TrueExtraction)
{
    Value v = Value::boolean(true);
    EXPECT_TRUE(v.isBool());
    EXPECT_TRUE(v.asBool());
}

TEST(ValueBool, FalseExtraction)
{
    Value v = Value::boolean(false);
    EXPECT_TRUE(v.isBool());
    EXPECT_FALSE(v.asBool());
}

TEST(ValueBool, TrueNotEqualFalse)
{
    EXPECT_NE(Value::boolean(true), Value::boolean(false));
}

TEST(ValueBool, SameValueEqual)
{
    EXPECT_EQ(Value::boolean(true), Value::boolean(true));
    EXPECT_EQ(Value::boolean(false), Value::boolean(false));
}

// Truthiness

TEST(Truthiness, NilIsFalsy) { EXPECT_FALSE(Value::nil().isTruthy()); }
TEST(Truthiness, FalseIsFalsy) { EXPECT_FALSE(Value::boolean(false).isTruthy()); }
TEST(Truthiness, TrueIsTruthy) { EXPECT_TRUE(Value::boolean(true).isTruthy()); }
TEST(Truthiness, ZeroIntIsFalsy) { EXPECT_FALSE(Value::integer(0).isTruthy()); }
TEST(Truthiness, NonZeroIntIsTruthy) { EXPECT_TRUE(Value::integer(1).isTruthy()); }
TEST(Truthiness, NegIntIsTruthy) { EXPECT_TRUE(Value::integer(-1).isTruthy()); }
TEST(Truthiness, ZeroDoubleIsFalsy) { EXPECT_FALSE(Value::real(0.0).isTruthy()); }
TEST(Truthiness, NonZeroDoubleIsTruthy) { EXPECT_TRUE(Value::real(0.001).isTruthy()); }
TEST(Truthiness, ObjIsTruthy)
{
    ObjList list;
    EXPECT_TRUE(Value::object(&list).isTruthy());
}

// Equality — raw bit comparison

TEST(ValueEquality, NilEqualsNil) { EXPECT_EQ(Value::nil(), Value::nil()); }
TEST(ValueEquality, NilNotEqualBool) { EXPECT_NE(Value::nil(), Value::boolean(false)); }
TEST(ValueEquality, SameIntEqual) { EXPECT_EQ(Value::integer(7), Value::integer(7)); }
TEST(ValueEquality, DiffIntNotEqual) { EXPECT_NE(Value::integer(7), Value::integer(8)); }
TEST(ValueEquality, SameDoubleEqual) { EXPECT_EQ(Value::real(1.0), Value::real(1.0)); }
TEST(ValueEquality, IntAndDoubleNotEqual)
{
    // int(1) and real(1.0) have different bit representations
    EXPECT_NE(Value::integer(1), Value::real(1.0));
}

// Heap object type discrimination

TEST(ObjTypes, StringType)
{
    ObjString s("hello");
    Value v = Value::object(&s);
    EXPECT_TRUE(v.isObj());
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isList());
    EXPECT_FALSE(v.isFunction());
    EXPECT_FALSE(v.isClosure());
    EXPECT_FALSE(v.isNative());
    EXPECT_EQ(v.asString(), &s);
}

TEST(ObjTypes, ListType)
{
    ObjList list;
    Value v = Value::object(&list);
    EXPECT_TRUE(v.isList());
    EXPECT_FALSE(v.isString());
    EXPECT_EQ(v.asList(), &list);
}

TEST(ObjTypes, NativeType)
{
    ObjString name("fn");
    ObjNative nat([](int, Value*) { return Value::nil(); }, &name, 0);
    Value v = Value::object(&nat);
    EXPECT_TRUE(v.isNative());
    EXPECT_FALSE(v.isClosure());
    EXPECT_EQ(v.asNative(), &nat);
}

// ObjHeader GC fields

TEST(ObjHeader, DefaultUnmarked)
{
    ObjList list;
    EXPECT_FALSE(list.is_marked);
    EXPECT_EQ(list.next, nullptr);
    EXPECT_EQ(list.type, ObjType::LIST);
}

TEST(ObjHeader, MarkBitToggle)
{
    ObjList list;
    list.is_marked = true;
    EXPECT_TRUE(list.is_marked);
    list.is_marked = false;
    EXPECT_FALSE(list.is_marked);
}

// ObjString

TEST(ObjString, TypeIsString)
{
    ObjString s("test");
    EXPECT_EQ(s.type, ObjType::STRING);
}

TEST(ObjString, CharsPreserved)
{
    ObjString s("hello world");
    EXPECT_EQ(s.chars, "hello world");
}

TEST(ObjString, EmptyString)
{
    ObjString s("");
    EXPECT_EQ(s.chars, "");
}

// ObjList

TEST(ObjList, StartsEmpty)
{
    ObjList list;
    EXPECT_TRUE(list.elements.empty());
    EXPECT_EQ(list.type, ObjType::LIST);
}

TEST(ObjList, CanAppendValues)
{
    ObjList list;
    list.elements.push_back(Value::integer(1));
    list.elements.push_back(Value::boolean(true));
    EXPECT_EQ(list.elements.size(), 2u);
    EXPECT_EQ(list.elements[0].asInt(), 1);
    EXPECT_TRUE(list.elements[1].asBool());
}

// ObjUpvalue

TEST(ObjUpvalue, PointsToStack)
{
    Value stack_val = Value::integer(42);
    ObjUpvalue uv(&stack_val);
    EXPECT_EQ(uv.type, ObjType::UPVALUE);
    EXPECT_EQ(uv.location, &stack_val);
    EXPECT_EQ(uv.next_open, nullptr);
}

TEST(ObjUpvalue, ClosedValueDefault)
{
    Value stack_val = Value::nil();
    ObjUpvalue uv(&stack_val);
    EXPECT_TRUE(uv.closed.isNil());
}

// TypeTag bitmask operations

TEST(TypeTag, Combine)
{
    TypeTag t = TypeTag::INT | TypeTag::DOUBLE;
    EXPECT_TRUE(has_tag(t, TypeTag::INT));
    EXPECT_TRUE(has_tag(t, TypeTag::DOUBLE));
    EXPECT_FALSE(has_tag(t, TypeTag::STRING));
}

TEST(TypeTag, NoneHasNoTags)
{
    TypeTag t = TypeTag::NONE;
    EXPECT_FALSE(has_tag(t, TypeTag::INT));
    EXPECT_FALSE(has_tag(t, TypeTag::BOOL));
    EXPECT_FALSE(has_tag(t, TypeTag::NIL));
}

TEST(TypeTag, OrAssign)
{
    TypeTag t = TypeTag::NONE;
    t |= TypeTag::STRING;
    t |= TypeTag::LIST;
    EXPECT_TRUE(has_tag(t, TypeTag::STRING));
    EXPECT_TRUE(has_tag(t, TypeTag::LIST));
    EXPECT_FALSE(has_tag(t, TypeTag::INT));
}

TEST(TypeTag, ValueTypeTagNil) { EXPECT_EQ(value_type_tag(Value::nil()), TypeTag::NIL); }
TEST(TypeTag, ValueTypeTagBool) { EXPECT_EQ(value_type_tag(Value::boolean(true)), TypeTag::BOOL); }
TEST(TypeTag, ValueTypeTagInt) { EXPECT_EQ(value_type_tag(Value::integer(1)), TypeTag::INT); }
TEST(TypeTag, ValueTypeTagDouble) { EXPECT_EQ(value_type_tag(Value::real(1.5)), TypeTag::DOUBLE); }
TEST(TypeTag, ValueTypeTagString)
{
    ObjString s("x");
    EXPECT_EQ(value_type_tag(Value::object(&s)), TypeTag::STRING);
}

TEST(TypeTag, ValueTypeTagList)
{
    ObjList l;
    EXPECT_EQ(value_type_tag(Value::object(&l)), TypeTag::LIST);
}
