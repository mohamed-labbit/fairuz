#pragma once


#include "../../../utfcpp/source/utf8.h"
#include "../../macros.hpp"
#include "../../types/string.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>


namespace mylang {
namespace runtime {
namespace object {

class Value
{
 public:
  enum class Type {
    NONE,
    INT,
    FLOAT,
    STRING,
    BOOL,
    LIST,
    DICT,
    TUPLE,
    SET,
    FUNCTION,
    NATIVE_FUNCTION,
    OBJECT,
    SLICE,
    ITERATOR,
    GENERATOR,
    COROUTINE
  };

  struct Function
  {
    std::int32_t           CodeOffset;
    std::vector<StringRef> params;
    std::vector<Value>     defaults;
    std::vector<Value>     closure;  // Captured variables
  };

  struct NativeFunction
  {
    std::function<Value(const std::vector<Value>&)> func;
    StringRef                                       name;
    std::int32_t                                    arity;
  };

  struct Object
  {
    StringRef                                                           className;
    std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> attributes;
    std::shared_ptr<Value>                                              parent;  // For inheritance
  };

  struct Iterator
  {
    std::vector<Value> items;
    SizeType           index;
  };

  using ValueData = std::variant<std::monostate,                                                       // None
                                 std::int64_t,                                                         // Int
                                 double,                                                               // Float
                                 StringRef,                                                            // String (shared for efficiency)
                                 bool,                                                                 // Bool
                                 std::vector<Value>,                                                   // List (shared)
                                 std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>,  // Dict
                                 Function,                                                             // User function
                                 NativeFunction,                                                       // Native C++ function
                                 Object,                                                               // Object instance
                                 Iterator                                                              // Iterator
                                 >;

 private:
  Type Type_;

  ValueData Data_;

  // Reference counting for memory management
  mutable std::int32_t RefCount_ = 0;

 public:
  Value() :
      Type_(Type::NONE)
  {
  }
  Value(std::int64_t v) :
      Type_(Type::INT),
      Data_(std::in_place_type<std::int64_t>, v)
  {
  }
  Value(double v) :
      Type_(Type::FLOAT),
      Data_(std::in_place_type<double>, v)
  {
  }
  Value(const StringRef& v) :
      Type_(Type::STRING),
      Data_(std::in_place_type<StringRef>, v)
  {
  }
  Value(bool v) :
      Type_(Type::BOOL),
      Data_(std::in_place_type<bool>, v)
  {
  }
  Value(std::vector<Value> v) :
      Type_(Type::LIST),
      Data_(std::in_place_type<std::vector<Value>>, v)
  {
  }
  Value(std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual> v) :
      Type_(Type::DICT),
      Data_(std::in_place_type<std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>>, v)
  {
  }

  // Copy constructor with COW (Copy-On-Write)
  Value(const Value& other) :
      Type_(other.Type_),
      Data_(other.Data_)
  {
  }

  Type getType() const { return Type_; }

  void setType(const Type type) { Type_ = type; }

  void setData(const std::variant<std::monostate,                                                       // None
                                  std::int64_t,                                                         // Int
                                  double,                                                               // Float
                                  StringRef,                                                            // String (shared for efficiency)
                                  bool,                                                                 // Bool
                                  std::vector<Value>,                                                   // List (shared)
                                  std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>,  // Dict
                                  Function,                                                             // User function
                                  NativeFunction,                                                       // Native C++ function
                                  Object,                                                               // Object instance
                                  Iterator                                                              // Iterator
                                  > data)
  {
    Data_ = data;
  }
  // Type checks
  bool isNone() const { return Type_ == Type::NONE; }

  bool isInt() const { return Type_ == Type::INT; }

  bool isFloat() const { return Type_ == Type::FLOAT; }

  bool isNumber() const { return isInt() || isFloat(); }

  bool isString() const { return Type_ == Type::STRING; }

  bool isBool() const { return Type_ == Type::BOOL; }

  bool isList() const { return Type_ == Type::LIST; }

  bool isDict() const { return Type_ == Type::DICT; }

  bool isFunction() const { return Type_ == Type::FUNCTION; }

  bool isCallable() const { return isFunction() || Type_ == Type::NATIVE_FUNCTION; }

  bool isIterable() const { return isList() || isString() || isDict(); }

  // Getters with safety
  std::int64_t asInt() const;

  double asFloat() const;

  const StringRef& asString() const;

  bool asBool() const;

  std::vector<Value>& asList();

  const std::vector<Value>& asList() const;

  std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>& asDict();

  const std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>& asDict() const;

  Function& asFunction();

  NativeFunction& asNativeFunction();

  double toFloat() const;  // Type conversions

  std::int64_t toInt() const;

  bool toBool() const;

  StringRef toString() const;

  std::string repr() const;

  SizeType hash() const;  // Hash for use in dictionaries

  bool hasNext() const;

  // Comparison operators
  bool operator==(const Value& other) const;
  bool operator<(const Value& other) const;
  bool operator>(const Value& other) const;
  bool operator<=(const Value& other) const;
  bool operator>=(const Value& other) const;
  bool operator!=(const Value& other) const;

  // Arithmetic operators with type promotion
  Value operator+(const Value& other) const;
  Value operator-(const Value& other) const;
  Value operator*(const Value& other) const;
  Value operator/(const Value& other) const;
  Value operator%(const Value& other) const;
  Value pow(const Value& other) const;
  Value operator-() const;  // Unary operators
  Value operator!() const;
  Value getItem(const Value& key) const;  // Subscript operator

  void  setItem(const Value& key, const Value& value);
  Value getIterator() const;
  Value next();

  static Value makeList(std::vector<Value> list);
};

}
}
}
