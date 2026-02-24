#include "../../include/IR/value.hpp"
#include "../../include/types/string.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using namespace mylang::IR;
using namespace mylang;

// Basic Type Construction Tests

class ValueConstructionTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(ValueConstructionTest, DefaultConstructorCreatesNone)
{
    Value v;
    EXPECT_TRUE(v.isNone());
    EXPECT_FALSE(v.isInt());
    EXPECT_FALSE(v.isFloat());
    EXPECT_FALSE(v.isString());
    EXPECT_FALSE(v.isBool());
    EXPECT_FALSE(v.isList());
    EXPECT_FALSE(v.isDict());
}

TEST_F(ValueConstructionTest, IntConstructor)
{
    Value v(42);
    EXPECT_TRUE(v.isInt());
    EXPECT_TRUE(v.isNumber());
    EXPECT_FALSE(v.isFloat());
    EXPECT_EQ(*v.asInt(), 42);
}

TEST_F(ValueConstructionTest, IntConstructorNegative)
{
    Value v(-100);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(*v.asInt(), -100);
}

TEST_F(ValueConstructionTest, IntConstructorZero)
{
    Value v(0);
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(*v.asInt(), 0);
}

TEST_F(ValueConstructionTest, IntConstructorMaxValue)
{
    Value v(std::numeric_limits<int64_t>::max());
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(*v.asInt(), std::numeric_limits<int64_t>::max());
}

TEST_F(ValueConstructionTest, IntConstructorMinValue)
{
    Value v(std::numeric_limits<int64_t>::min());
    EXPECT_TRUE(v.isInt());
    EXPECT_EQ(*v.asInt(), std::numeric_limits<int64_t>::min());
}

TEST_F(ValueConstructionTest, FloatConstructor)
{
    Value v(3.14);
    EXPECT_TRUE(v.isFloat());
    EXPECT_TRUE(v.isNumber());
    EXPECT_FALSE(v.isInt());
    EXPECT_DOUBLE_EQ(*v.asFloat(), 3.14);
}

TEST_F(ValueConstructionTest, FloatConstructorNegative)
{
    Value v(-2.718);
    EXPECT_TRUE(v.isFloat());
    EXPECT_DOUBLE_EQ(*v.asFloat(), -2.718);
}

TEST_F(ValueConstructionTest, FloatConstructorZero)
{
    Value v(0.0);
    EXPECT_TRUE(v.isFloat());
    EXPECT_DOUBLE_EQ(*v.asFloat(), 0.0);
}

TEST_F(ValueConstructionTest, FloatConstructorInfinity)
{
    Value v(std::numeric_limits<double>::infinity());
    EXPECT_TRUE(v.isFloat());
    EXPECT_TRUE(std::isinf(*v.asFloat()));
}

TEST_F(ValueConstructionTest, FloatConstructorNaN)
{
    Value v(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(v.isFloat());
    EXPECT_TRUE(std::isnan(*v.asFloat()));
}

TEST_F(ValueConstructionTest, BoolConstructorTrue)
{
    Value v(true);
    EXPECT_TRUE(v.isBool());
    EXPECT_FALSE(v.isInt());
    EXPECT_TRUE(*v.asBool());
}

TEST_F(ValueConstructionTest, BoolConstructorFalse)
{
    Value v(false);
    EXPECT_TRUE(v.isBool());
    EXPECT_FALSE(*v.asBool());
}

TEST_F(ValueConstructionTest, StringConstructor)
{
    StringRef str("hello");
    Value v(str);
    EXPECT_TRUE(v.isString());
    EXPECT_FALSE(v.isInt());
    EXPECT_EQ(*v.asString(), str);
}

TEST_F(ValueConstructionTest, StringConstructorEmpty)
{
    StringRef str("");
    Value v(str);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->len(), 0);
}

TEST_F(ValueConstructionTest, ListConstructorEmpty)
{
    std::vector<Value> list;
    Value v(list);
    EXPECT_TRUE(v.isList());
    EXPECT_TRUE(v.isIterable());
    EXPECT_EQ(v.asList()->size(), 0);
}

TEST_F(ValueConstructionTest, ListConstructorWithElements)
{
    std::vector<Value> list = { Value(1), Value(2), Value(3) };
    Value v(list);
    EXPECT_TRUE(v.isList());
    EXPECT_EQ(v.asList()->size(), 3);
    EXPECT_EQ(*(*v.asList())[0].asInt(), 1);
    EXPECT_EQ(*(*v.asList())[1].asInt(), 2);
    EXPECT_EQ(*(*v.asList())[2].asInt(), 3);
}

TEST_F(ValueConstructionTest, ListConstructorMixedTypes)
{
    std::vector<Value> list = { Value(42), Value(3.14), Value(true), Value(StringRef("test")) };
    Value v(list);
    EXPECT_TRUE(v.isList());
    EXPECT_EQ(v.asList()->size(), 4);
    EXPECT_TRUE((*v.asList())[0].isInt());
    EXPECT_TRUE((*v.asList())[1].isFloat());
    EXPECT_TRUE((*v.asList())[2].isBool());
    EXPECT_TRUE((*v.asList())[3].isString());
}

TEST_F(ValueConstructionTest, DictConstructorEmpty)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    Value v(dict);
    EXPECT_TRUE(v.isDict());
    EXPECT_TRUE(v.isIterable());
    EXPECT_EQ(v.asDict()->size(), 0);
}

TEST_F(ValueConstructionTest, DictConstructorWithElements)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("key1")] = Value(42);
    dict[StringRef("key2")] = Value(3.14);
    Value v(dict);
    EXPECT_TRUE(v.isDict());
    EXPECT_EQ(v.asDict()->size(), 2);
}

TEST_F(ValueConstructionTest, CopyConstructor)
{
    Value original(42);
    Value copy(original);
    EXPECT_TRUE(copy.isInt());
    EXPECT_EQ(*copy.asInt(), 42);
}

TEST_F(ValueConstructionTest, CopyConstructorList)
{
    std::vector<Value> list = { Value(1), Value(2), Value(3) };
    Value original(list);
    Value copy(original);
    EXPECT_TRUE(copy.isList());
    EXPECT_EQ(copy.asList()->size(), 3);
}

// Type Checking Tests

class ValueTypeCheckTest : public ::testing::Test { };

TEST_F(ValueTypeCheckTest, IsNumberForInt)
{
    Value v(42);
    EXPECT_TRUE(v.isNumber());
}

TEST_F(ValueTypeCheckTest, IsNumberForFloat)
{
    Value v(3.14);
    EXPECT_TRUE(v.isNumber());
}

TEST_F(ValueTypeCheckTest, IsNumberForNonNumeric)
{
    Value v(true);
    EXPECT_FALSE(v.isNumber());
}

TEST_F(ValueTypeCheckTest, IsIterableForList)
{
    Value v(std::vector<Value> {});
    EXPECT_TRUE(v.isIterable());
}

TEST_F(ValueTypeCheckTest, IsIterableForString)
{
    Value v(StringRef("test"));
    EXPECT_TRUE(v.isIterable());
}

TEST_F(ValueTypeCheckTest, IsIterableForDict)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    Value v(dict);
    EXPECT_TRUE(v.isIterable());
}

TEST_F(ValueTypeCheckTest, IsIterableForNonIterable)
{
    Value v(42);
    EXPECT_FALSE(v.isIterable());
}

TEST_F(ValueTypeCheckTest, GetTypeReturnsCorrectType)
{
    EXPECT_EQ(Value().getType(), Value::Type::NONE);
    EXPECT_EQ(Value(42).getType(), Value::Type::INT);
    EXPECT_EQ(Value(3.14).getType(), Value::Type::FLOAT);
    EXPECT_EQ(Value(true).getType(), Value::Type::BOOL);
    EXPECT_EQ(Value(StringRef("test")).getType(), Value::Type::STRING);
}

// Size Tests

class ValueSizeTest : public ::testing::Test { };

TEST_F(ValueSizeTest, IntSize)
{
    Value v(42);
    EXPECT_EQ(v.size(), sizeof(int64_t));
}

TEST_F(ValueSizeTest, FloatSize)
{
    Value v(3.14);
    EXPECT_EQ(v.size(), sizeof(double));
}

TEST_F(ValueSizeTest, BoolSize)
{
    Value v(true);
    EXPECT_EQ(v.size(), sizeof(bool));
}

TEST_F(ValueSizeTest, StringSize)
{
    StringRef str("hello");
    Value v(str);
    EXPECT_EQ(v.size(), str.len());
}

TEST_F(ValueSizeTest, ListSize)
{
    std::vector<Value> list = { Value(1), Value(2), Value(3) };
    Value v(list);
    EXPECT_EQ(v.size(), 3);
}

TEST_F(ValueSizeTest, DictSize)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("a")] = Value(1);
    dict[StringRef("b")] = Value(2);
    Value v(dict);
    EXPECT_EQ(v.size(), 2);
}

TEST_F(ValueSizeTest, EmptyListSize)
{
    Value v(std::vector<Value> {});
    EXPECT_EQ(v.size(), 0);
}

// Type Conversion Tests

class ValueTypeConversionTest : public ::testing::Test { };

TEST_F(ValueTypeConversionTest, IntToFloat)
{
    Value v(42);
    EXPECT_DOUBLE_EQ(v.toFloat(), 42.0);
}

TEST_F(ValueTypeConversionTest, FloatToInt)
{
    Value v(3.14);
    EXPECT_EQ(v.toInt(), 3);
}

TEST_F(ValueTypeConversionTest, FloatToIntNegative)
{
    Value v(-2.7);
    EXPECT_EQ(v.toInt(), -2);
}

TEST_F(ValueTypeConversionTest, IntToBoolTrue)
{
    Value v(42);
    EXPECT_TRUE(v.toBool());
}

TEST_F(ValueTypeConversionTest, IntToBoolFalse)
{
    Value v(0);
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueTypeConversionTest, FloatToBoolTrue)
{
    Value v(3.14);
    EXPECT_TRUE(v.toBool());
}

TEST_F(ValueTypeConversionTest, FloatToBoolFalse)
{
    Value v(0.0);
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueTypeConversionTest, StringToBoolEmpty)
{
    Value v(StringRef(""));
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueTypeConversionTest, StringToBoolNonEmpty)
{
    Value v(StringRef("hello"));
    EXPECT_TRUE(v.toBool());
}

TEST_F(ValueTypeConversionTest, ListToBoolEmpty)
{
    Value v(std::vector<Value> {});
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueTypeConversionTest, ListToBoolNonEmpty)
{
    Value v(std::vector<Value> { Value(1) });
    EXPECT_TRUE(v.toBool());
}

TEST_F(ValueTypeConversionTest, NoneToBool)
{
    Value v;
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueTypeConversionTest, BoolToIntTrue)
{
    Value v(true);
    EXPECT_EQ(v.toInt(), 1);
}

TEST_F(ValueTypeConversionTest, BoolToIntFalse)
{
    Value v(false);
    EXPECT_EQ(v.toInt(), 0);
}

// Comparison Operator Tests

class ValueComparisonTest : public ::testing::Test { };

TEST_F(ValueComparisonTest, IntEqualityTrue)
{
    Value v1(42);
    Value v2(42);
    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 != v2);
}

TEST_F(ValueComparisonTest, IntEqualityFalse)
{
    Value v1(42);
    Value v2(43);
    EXPECT_FALSE(v1 == v2);
    EXPECT_TRUE(v1 != v2);
}

TEST_F(ValueComparisonTest, FloatEquality)
{
    Value v1(3.14);
    Value v2(3.14);
    EXPECT_TRUE(v1 == v2);
}

TEST_F(ValueComparisonTest, BoolEquality)
{
    Value v1(true);
    Value v2(true);
    Value v3(false);
    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

TEST_F(ValueComparisonTest, NoneEquality)
{
    Value v1;
    Value v2;
    EXPECT_TRUE(v1 == v2);
}

TEST_F(ValueComparisonTest, MixedTypeEqualityIntFloat)
{
    Value v1(42);
    Value v2(42.0);
    Value v3("hello");
    EXPECT_TRUE(v1 == v2);  // different types but same val
    EXPECT_FALSE(v1 == v3); // different types and not numeric
}

TEST_F(ValueComparisonTest, IntLessThanTrue)
{
    Value v1(10);
    Value v2(20);
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v2 < v1);
}

TEST_F(ValueComparisonTest, IntLessThanFalse)
{
    Value v1(20);
    Value v2(20);
    EXPECT_FALSE(v1 < v2);
}

TEST_F(ValueComparisonTest, IntGreaterThan)
{
    Value v1(30);
    Value v2(20);
    EXPECT_TRUE(v1 > v2);
    EXPECT_FALSE(v2 > v1);
}

TEST_F(ValueComparisonTest, IntLessThanOrEqual)
{
    Value v1(20);
    Value v2(20);
    Value v3(21);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v3);
    EXPECT_FALSE(v3 <= v1);
}

TEST_F(ValueComparisonTest, IntGreaterThanOrEqual)
{
    Value v1(20);
    Value v2(20);
    Value v3(19);
    EXPECT_TRUE(v1 >= v2);
    EXPECT_TRUE(v1 >= v3);
    EXPECT_FALSE(v3 >= v1);
}

TEST_F(ValueComparisonTest, FloatComparison)
{
    Value v1(3.14);
    Value v2(2.71);
    EXPECT_TRUE(v1 > v2);
    EXPECT_FALSE(v1 < v2);
}

TEST_F(ValueComparisonTest, NegativeNumberComparison)
{
    Value v1(-10);
    Value v2(-5);
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v1 > v2);
}

// Arithmetic Operator Tests

class ValueArithmeticTest : public ::testing::Test { };

TEST_F(ValueArithmeticTest, IntAddition)
{
    Value v1(10);
    Value v2(20);
    Value result = v1 + v2;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), 30);
}

TEST_F(ValueArithmeticTest, IntAdditionNegative)
{
    Value v1(10);
    Value v2(-5);
    Value result = v1 + v2;
    EXPECT_EQ(*result.asInt(), 5);
}

TEST_F(ValueArithmeticTest, IntAdditionZero)
{
    Value v1(10);
    Value v2(0);
    Value result = v1 + v2;
    EXPECT_EQ(*result.asInt(), 10);
}

TEST_F(ValueArithmeticTest, FloatAddition)
{
    Value v1(3.14);
    Value v2(2.71);
    Value result = v1 + v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 5.85);
}

TEST_F(ValueArithmeticTest, IntFloatAdditionPromotion)
{
    Value v1(10);
    Value v2(3.14);
    Value result = v1 + v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 13.14);
}

TEST_F(ValueArithmeticTest, IntSubtraction)
{
    Value v1(20);
    Value v2(10);
    Value result = v1 - v2;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), 10);
}

TEST_F(ValueArithmeticTest, IntSubtractionNegativeResult)
{
    Value v1(10);
    Value v2(20);
    Value result = v1 - v2;
    EXPECT_EQ(*result.asInt(), -10);
}

TEST_F(ValueArithmeticTest, FloatSubtraction)
{
    Value v1(5.5);
    Value v2(2.5);
    Value result = v1 - v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 3.0);
}

TEST_F(ValueArithmeticTest, IntMultiplication)
{
    Value v1(6);
    Value v2(7);
    Value result = v1 * v2;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), 42);
}

TEST_F(ValueArithmeticTest, IntMultiplicationByZero)
{
    Value v1(42);
    Value v2(0);
    Value result = v1 * v2;
    EXPECT_EQ(*result.asInt(), 0);
}

TEST_F(ValueArithmeticTest, IntMultiplicationByNegative)
{
    Value v1(6);
    Value v2(-7);
    Value result = v1 * v2;
    EXPECT_EQ(*result.asInt(), -42);
}

TEST_F(ValueArithmeticTest, FloatMultiplication)
{
    Value v1(2.5);
    Value v2(4.0);
    Value result = v1 * v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 10.0);
}

TEST_F(ValueArithmeticTest, IntDivision)
{
    Value v1(20);
    Value v2(4);
    Value result = v1 / v2;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), 5);
}

TEST_F(ValueArithmeticTest, IntDivisionTruncation)
{
    Value v1(7);
    Value v2(2);
    Value result = v1 / v2;
    EXPECT_EQ(*result.asInt(), 3);
}

TEST_F(ValueArithmeticTest, FloatDivision)
{
    Value v1(7.0);
    Value v2(2.0);
    Value result = v1 / v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 3.5);
}

TEST_F(ValueArithmeticTest, IntModulo)
{
    Value v1(17);
    Value v2(5);
    Value result = v1 % v2;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), 2);
}

TEST_F(ValueArithmeticTest, IntModuloZeroRemainder)
{
    Value v1(20);
    Value v2(5);
    Value result = v1 % v2;
    EXPECT_EQ(*result.asInt(), 0);
}

TEST_F(ValueArithmeticTest, FloatModulo)
{
    Value v1(7.0);
    Value v2(2.5);
    Value result = v1 % v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 2.0);
}

TEST_F(ValueArithmeticTest, UnaryMinus)
{
    Value v(42);
    Value result = -v;
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(*result.asInt(), -42);
}

TEST_F(ValueArithmeticTest, UnaryMinusFloat)
{
    Value v(3.14);
    Value result = -v;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), -3.14);
}

TEST_F(ValueArithmeticTest, UnaryMinusNegative)
{
    Value v(-42);
    Value result = -v;
    EXPECT_EQ(*result.asInt(), 42);
}

TEST_F(ValueArithmeticTest, LogicalNot)
{
    Value v1(true);
    Value v2(false);
    Value result1 = !v1;
    Value result2 = !v2;
    EXPECT_TRUE(result1.isBool());
    EXPECT_FALSE(*result1.asBool());
    EXPECT_TRUE(*result2.asBool());
}

TEST_F(ValueArithmeticTest, PowerOperation)
{
    Value v1(2);
    Value v2(3);
    Value result = v1.pow(v2);
    EXPECT_TRUE(result.isNumber());
    EXPECT_EQ(result.toInt(), 8);
}

TEST_F(ValueArithmeticTest, PowerOperationFloat)
{
    Value v1(2.0);
    Value v2(3.0);
    Value result = v1.pow(v2);
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 8.0);
}

TEST_F(ValueArithmeticTest, PowerOperationZeroExponent)
{
    Value v1(42);
    Value v2(0);
    Value result = v1.pow(v2);
    EXPECT_EQ(result.toInt(), 1);
}

TEST_F(ValueArithmeticTest, PowerOperationNegativeExponent)
{
    Value v1(2.0);
    Value v2(-2.0);
    Value result = v1.pow(v2);
    EXPECT_DOUBLE_EQ(*result.asFloat(), 0.25);
}

TEST_F(ValueArithmeticTest, StringConcatenation)
{
    Value v1(StringRef("Hello, "));
    Value v2(StringRef("World!"));
    Value result = v1 + v2;
    EXPECT_TRUE(result.isString());
}

TEST_F(ValueArithmeticTest, ListConcatenation)
{
    std::vector<Value> list1 = { Value(1), Value(2) };
    std::vector<Value> list2 = { Value(3), Value(4) };
    Value v1(list1);
    Value v2(list2);
    Value result = v1 + v2;
    EXPECT_TRUE(result.isList());
    EXPECT_EQ(result.asList()->size(), 4);
}

// List Operations Tests

class ValueListOperationsTest : public ::testing::Test { };

TEST_F(ValueListOperationsTest, GetItemByIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    Value item = v.getItem(Value(1));
    EXPECT_EQ(*item.asInt(), 20);
}

TEST_F(ValueListOperationsTest, GetItemFirstIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    Value item = v.getItem(Value(0));
    EXPECT_EQ(*item.asInt(), 10);
}

TEST_F(ValueListOperationsTest, GetItemLastIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    Value item = v.getItem(Value(2));
    EXPECT_EQ(*item.asInt(), 30);
}

TEST_F(ValueListOperationsTest, GetItemNegativeIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    Value item = v.getItem(Value(-1));
    EXPECT_EQ(*item.asInt(), 30);
}

TEST_F(ValueListOperationsTest, SetItemByIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    v.setItem(Value(1), Value(99));
    EXPECT_EQ(*(*v.asList())[1].asInt(), 99);
}

TEST_F(ValueListOperationsTest, SetItemFirstIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    v.setItem(Value(0), Value(5));
    EXPECT_EQ(*(*v.asList())[0].asInt(), 5);
}

TEST_F(ValueListOperationsTest, SetItemLastIndex)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    v.setItem(Value(2), Value(100));
    EXPECT_EQ(*(*v.asList())[2].asInt(), 100);
}

TEST_F(ValueListOperationsTest, MakeList)
{
    std::vector<Value> elements = { Value(1), Value(2), Value(3) };
    Value v = Value::makeList(elements);
    EXPECT_TRUE(v.isList());
    EXPECT_EQ(v.asList()->size(), 3);
}

TEST_F(ValueListOperationsTest, MakeListEmpty)
{
    std::vector<Value> elements;
    Value v = Value::makeList(elements);
    EXPECT_TRUE(v.isList());
    EXPECT_EQ(v.asList()->size(), 0);
}

TEST_F(ValueListOperationsTest, ListModification)
{
    std::vector<Value> list = { Value(1) };
    Value v(list);
    v.asList()->push_back(Value(2));
    v.asList()->push_back(Value(3));
    EXPECT_EQ(v.asList()->size(), 3);
    EXPECT_EQ(*(*v.asList())[2].asInt(), 3);
}

// Dictionary Operations Tests

class ValueDictOperationsTest : public ::testing::Test { };

TEST_F(ValueDictOperationsTest, GetItemByKey)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("name")] = Value(StringRef("Alice"));
    dict[StringRef("age")] = Value(30);
    Value v(dict);

    Value name = v.getItem(Value(StringRef("name")));
    EXPECT_TRUE(name.isString());
}

TEST_F(ValueDictOperationsTest, SetItemByKey)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    Value v(dict);
    v.setItem(Value(StringRef("key")), Value(42));

    EXPECT_EQ(v.asDict()->size(), 1);
    Value item = v.getItem(Value(StringRef("key")));
    EXPECT_EQ(*item.asInt(), 42);
}

TEST_F(ValueDictOperationsTest, SetItemOverwriteExisting)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("key")] = Value(10);
    Value v(dict);

    v.setItem(Value(StringRef("key")), Value(20));
    Value item = v.getItem(Value(StringRef("key")));
    EXPECT_EQ(*item.asInt(), 20);
}

TEST_F(ValueDictOperationsTest, DictMultipleKeys)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("a")] = Value(1);
    dict[StringRef("b")] = Value(2);
    dict[StringRef("c")] = Value(3);
    Value v(dict);

    EXPECT_EQ(v.asDict()->size(), 3);
}

TEST_F(ValueDictOperationsTest, DictMixedValueTypes)
{
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("int")] = Value(42);
    dict[StringRef("float")] = Value(3.14);
    dict[StringRef("bool")] = Value(true);
    dict[StringRef("string")] = Value(StringRef("test"));
    Value v(dict);

    EXPECT_EQ(v.asDict()->size(), 4);
    EXPECT_TRUE(v.getItem(Value(StringRef("int"))).isInt());
    EXPECT_TRUE(v.getItem(Value(StringRef("float"))).isFloat());
    EXPECT_TRUE(v.getItem(Value(StringRef("bool"))).isBool());
    EXPECT_TRUE(v.getItem(Value(StringRef("string"))).isString());
}

// String Operations Tests

class ValueStringOperationsTest : public ::testing::Test { };

TEST_F(ValueStringOperationsTest, StringIndexing)
{
    Value v(StringRef("hello"));
    Value ch = v.getItem(Value(0));
    EXPECT_TRUE(ch.isString());
}

TEST_F(ValueStringOperationsTest, StringNegativeIndexing)
{
    Value v(StringRef("hello"));
    Value ch = v.getItem(Value(-1));
    EXPECT_TRUE(ch.isString());
}

TEST_F(ValueStringOperationsTest, StringToString)
{
    Value v(StringRef("test"));
    StringRef str = v.toString();
    EXPECT_EQ(str.len(), 4);
}

TEST_F(ValueStringOperationsTest, IntToString)
{
    Value v(42);
    StringRef str = v.toString();
    EXPECT_GT(str.len(), 0);
}

TEST_F(ValueStringOperationsTest, FloatToString)
{
    Value v(3.14);
    StringRef str = v.toString();
    EXPECT_GT(str.len(), 0);
}

TEST_F(ValueStringOperationsTest, BoolToString)
{
    Value v(true);
    StringRef str = v.toString();
    EXPECT_GT(str.len(), 0);
}

TEST_F(ValueStringOperationsTest, NoneToString)
{
    Value v;
    StringRef str = v.toString();
    EXPECT_GT(str.len(), 0);
}

// Iterator Tests

class ValueIteratorTest : public ::testing::Test { };

TEST_F(ValueIteratorTest, GetIteratorFromList)
{
    std::vector<Value> list = { Value(1), Value(2), Value(3) };
    Value v(list);
    Value iter = v.getIterator();
    EXPECT_TRUE(iter.hasNext());
}

TEST_F(ValueIteratorTest, IterateList)
{
    std::vector<Value> list = { Value(10), Value(20), Value(30) };
    Value v(list);
    Value iter = v.getIterator();

    Value item1 = iter.next();
    EXPECT_EQ(*item1.asInt(), 10);
    EXPECT_TRUE(iter.hasNext());

    Value item2 = iter.next();
    EXPECT_EQ(*item2.asInt(), 20);
    EXPECT_TRUE(iter.hasNext());

    Value item3 = iter.next();
    EXPECT_EQ(*item3.asInt(), 30);
    EXPECT_FALSE(iter.hasNext());
}

TEST_F(ValueIteratorTest, IterateEmptyList)
{
    std::vector<Value> list;
    Value v(list);
    Value iter = v.getIterator();
    EXPECT_FALSE(iter.hasNext());
}

TEST_F(ValueIteratorTest, GetIteratorFromString)
{
    GTEST_SKIP() << "GetIteratorFromString: Not supported yet";
    Value v(StringRef("abc"));
    Value iter = v.getIterator();
    EXPECT_TRUE(iter.hasNext());
}

TEST_F(ValueIteratorTest, GetIteratorFromDict)
{
    GTEST_SKIP() << "GetIteratorFromDict: Not supported yet";
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> dict;
    dict[StringRef("key")] = Value(42);
    Value v(dict);
    Value iter = v.getIterator();
    EXPECT_TRUE(iter.hasNext());
}

TEST_F(ValueIteratorTest, HasNextSingleElement)
{
    std::vector<Value> list = { Value(1) };
    Value v(list);
    Value iter = v.getIterator();
    EXPECT_TRUE(iter.hasNext());
    iter.next();
    EXPECT_FALSE(iter.hasNext());
}

// Hash Tests

class ValueHashTest : public ::testing::Test { };

TEST_F(ValueHashTest, IntHash)
{
    Value v1(42);
    Value v2(42);
    EXPECT_EQ(v1.hash(), v2.hash());
}

TEST_F(ValueHashTest, IntHashDifferent)
{
    Value v1(42);
    Value v2(43);
    EXPECT_NE(v1.hash(), v2.hash());
}

TEST_F(ValueHashTest, FloatHash)
{
    Value v1(3.14);
    Value v2(3.14);
    EXPECT_EQ(v1.hash(), v2.hash());
}

TEST_F(ValueHashTest, StringHash)
{
    Value v1(StringRef("hello"));
    Value v2(StringRef("hello"));
    EXPECT_EQ(v1.hash(), v2.hash());
}

TEST_F(ValueHashTest, BoolHash)
{
    Value v1(true);
    Value v2(true);
    EXPECT_EQ(v1.hash(), v2.hash());
}

TEST_F(ValueHashTest, DifferentTypesHash)
{
    Value v1(42);
    Value v2(42.0);
    // Different types should likely have different hashes
    // But this depends on implementation
}

// Repr Tests

class ValueReprTest : public ::testing::Test { };

TEST_F(ValueReprTest, IntRepr)
{
    Value v(42);
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, FloatRepr)
{
    Value v(3.14);
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, BoolRepr)
{
    Value v(true);
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, StringRepr)
{
    Value v(StringRef("test"));
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, NoneRepr)
{
    Value v;
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, ListRepr)
{
    std::vector<Value> list = { Value(1), Value(2) };
    Value v(list);
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

TEST_F(ValueReprTest, EmptyListRepr)
{
    Value v(std::vector<Value> {});
    std::string repr = v.repr();
    EXPECT_FALSE(repr.empty());
}

// Edge Cases and Stress Tests

class ValueEdgeCaseTest : public ::testing::Test { };

TEST_F(ValueEdgeCaseTest, LargeListCreation)
{
    std::vector<Value> large_list;
    for (int i = 0; i < 10000; ++i) {
        large_list.push_back(Value(i));
    }
    Value v(large_list);
    EXPECT_TRUE(v.isList());
    EXPECT_EQ(v.asList()->size(), 10000);
}

TEST_F(ValueEdgeCaseTest, DeepListNesting)
{
    Value innermost(42);
    std::vector<Value> list1 = { innermost };
    std::vector<Value> list2 = { Value(list1) };
    std::vector<Value> list3 = { Value(list2) };
    Value v(list3);

    EXPECT_TRUE(v.isList());
    EXPECT_TRUE((*v.asList())[0].isList());
}

TEST_F(ValueEdgeCaseTest, MixedOperationsSequence)
{
    Value v(10);
    v = v + Value(5);
    v = v * Value(2);
    v = v - Value(10);
    EXPECT_EQ(*v.asInt(), 20);
}

TEST_F(ValueEdgeCaseTest, ChainedComparisons)
{
    Value v1(10);
    Value v2(20);
    Value v3(30);

    EXPECT_TRUE(v1 < v2 && v2 < v3);
    EXPECT_TRUE(v3 > v2 && v2 > v1);
}

TEST_F(ValueEdgeCaseTest, ZeroDivisionBehavior)
{
    Value v1(42);
    Value v2(0);
    // This test documents behavior - implementation may throw or return special value
    // EXPECT_THROW(v1 / v2, std::exception); // If implementation throws
    // Or just document that undefined behavior occurs
}

TEST_F(ValueEdgeCaseTest, OverflowBehavior)
{
    Value v1(std::numeric_limits<int64_t>::max());
    Value v2(1);
    // Document overflow behavior
    Value result = v1 + v2;
    // Behavior is implementation-defined
}

TEST_F(ValueEdgeCaseTest, UnderflowBehavior)
{
    Value v1(std::numeric_limits<int64_t>::min());
    Value v2(1);
    Value result = v1 - v2;
    // Behavior is implementation-defined
}

TEST_F(ValueEdgeCaseTest, FloatPrecisionLoss)
{
    Value v1(0.1);
    Value v2(0.2);
    Value result = v1 + v2;
    // Floating point precision issues are expected
    EXPECT_TRUE(result.isFloat());
}

TEST_F(ValueEdgeCaseTest, LargeExponentiation)
{
    Value v1(2);
    Value v2(10);
    Value result = v1.pow(v2);
    EXPECT_EQ(result.toInt(), 1024);
}

TEST_F(ValueEdgeCaseTest, NegativeBasePositiveExponent)
{
    Value v1(-2);
    Value v2(3);
    Value result = v1.pow(v2);
    EXPECT_EQ(result.toInt(), -8);
}

TEST_F(ValueEdgeCaseTest, EmptyStringOperations)
{
    Value v(StringRef(""));
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.size(), 0);
    EXPECT_FALSE(v.toBool());
}

TEST_F(ValueEdgeCaseTest, MultipleTypeConversions)
{
    Value v(42);
    double f = v.toFloat();
    Value v2(f);
    int i = v2.toInt();
    EXPECT_EQ(i, 42);
}

TEST_F(ValueEdgeCaseTest, ListWithDifferentTypes)
{
    std::vector<Value> list = {
        Value(42),
        Value(3.14),
        Value(true),
        Value(StringRef("test")),
        Value(std::vector<Value> {}),
        Value()
    };
    Value v(list);
    EXPECT_EQ(v.asList()->size(), 6);
    EXPECT_TRUE((*v.asList())[0].isInt());
    EXPECT_TRUE((*v.asList())[1].isFloat());
    EXPECT_TRUE((*v.asList())[2].isBool());
    EXPECT_TRUE((*v.asList())[3].isString());
    EXPECT_TRUE((*v.asList())[4].isList());
    EXPECT_TRUE((*v.asList())[5].isNone());
}

TEST_F(ValueEdgeCaseTest, CopyIndependence)
{
    std::vector<Value> list = { Value(1), Value(2) };
    Value original(list);
    Value copy(original);

    // Modify copy
    copy.asList()->push_back(Value(3));

    // Check that original is affected (or not, depending on COW implementation)
    // This test documents the COW behavior
}

TEST_F(ValueEdgeCaseTest, SelfAssignment)
{
    Value v(42);
    v = v;
    EXPECT_EQ(*v.asInt(), 42);
}

TEST_F(ValueEdgeCaseTest, SelfComparison)
{
    Value v(42);
    EXPECT_TRUE(v == v);
    EXPECT_FALSE(v != v);
    EXPECT_FALSE(v < v);
    EXPECT_FALSE(v > v);
    EXPECT_TRUE(v <= v);
    EXPECT_TRUE(v >= v);
}

TEST_F(ValueEdgeCaseTest, SelfArithmetic)
{
    Value v(5);
    Value result = v + v;
    EXPECT_EQ(*result.asInt(), 10);

    result = v * v;
    EXPECT_EQ(*result.asInt(), 25);
}

// Function and Callable Tests

class ValueFunctionTest : public ::testing::Test { };

TEST_F(ValueFunctionTest, IsCallableForFunction)
{
    Value v;
    v.setType(Value::Type::FUNCTION);
    EXPECT_TRUE(v.isFunction());
    EXPECT_TRUE(v.isCallable());
}

TEST_F(ValueFunctionTest, IsCallableForNativeFunction)
{
    Value v;
    v.setType(Value::Type::NATIVE_FUNCTION);
    EXPECT_TRUE(v.isCallable());
    EXPECT_FALSE(v.isFunction());
}

TEST_F(ValueFunctionTest, IsCallableForNonFunction)
{
    Value v(42);
    EXPECT_FALSE(v.isCallable());
}

// Performance and Boundary Tests

class ValuePerformanceTest : public ::testing::Test { };

TEST_F(ValuePerformanceTest, ManySmallOperations)
{
    Value result(0);
    for (int i = 0; i < 1000; ++i) {
        result = result + Value(1);
    }
    EXPECT_EQ(*result.asInt(), 1000);
}

TEST_F(ValuePerformanceTest, LargeNumberArithmetic)
{
    Value v1(1000000);
    Value v2(999999);
    Value result = v1 * v2;
    EXPECT_TRUE(result.isInt());
}

TEST_F(ValuePerformanceTest, DeepCopyChain)
{
    Value v1(42);
    Value v2(v1);
    Value v3(v2);
    Value v4(v3);
    Value v5(v4);
    EXPECT_EQ(*v5.asInt(), 42);
}

// Type Mutation Tests

class ValueTypeMutationTest : public ::testing::Test { };

TEST_F(ValueTypeMutationTest, SetTypeChangesType)
{
    Value v(42);
    EXPECT_TRUE(v.isInt());
    v.setType(Value::Type::FLOAT);
    EXPECT_TRUE(v.isFloat());
    EXPECT_FALSE(v.isInt());
}

TEST_F(ValueTypeMutationTest, SetTypeToNone)
{
    Value v(42);
    v.setType(Value::Type::NONE);
    EXPECT_TRUE(v.isNone());
}

TEST_F(ValueTypeMutationTest, SetDataChangesInternalData)
{
    Value v(42);
    v.setData(int64_t(100));
    // After setData, the internal data should reflect the change
    // Actual behavior depends on implementation
}

// Comparison with Different Numeric Types

class ValueMixedNumericTest : public ::testing::Test { };

TEST_F(ValueMixedNumericTest, IntAndFloatAddition)
{
    Value v1(10);
    Value v2(3.5);
    Value result = v1 + v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 13.5);
}

TEST_F(ValueMixedNumericTest, FloatAndIntSubtraction)
{
    Value v1(10.5);
    Value v2(2);
    Value result = v1 - v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 8.5);
}

TEST_F(ValueMixedNumericTest, IntAndFloatMultiplication)
{
    Value v1(5);
    Value v2(2.5);
    Value result = v1 * v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 12.5);
}

TEST_F(ValueMixedNumericTest, FloatAndIntDivision)
{
    Value v1(10.0);
    Value v2(4);
    Value result = v1 / v2;
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 2.5);
}
