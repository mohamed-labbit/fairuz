#pragma once


#include "../../../utfcpp/source/utf8.h"
#include "../../macros.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>


namespace mylang {
namespace runtime {
namespace object {

// ============================================================================
// ADVANCED VALUE SYSTEM - Full Python-like dynamic typing
// ============================================================================
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


 private:
  Type type_;

  struct Function
  {
    std::int32_t codeOffset;
    std::vector<string_type> params;
    std::vector<Value> defaults;
    std::vector<Value> closure;  // Captured variables
  };

  struct NativeFunction
  {
    std::function<Value(const std::vector<Value>&)> func;
    string_type name;
    std::int32_t arity;
  };

  struct Object
  {
    string_type className;
    std::unordered_map<string_type, Value> attributes;
    std::shared_ptr<Value> parent;  // For inheritance
  };

  struct Iterator
  {
    std::shared_ptr<std::vector<Value>> items;
    std::size_t index;
  };

  std::variant<std::monostate,  // None
    std::int64_t,  // Int
    double,  // Float
    std::shared_ptr<string_type>,  // String (shared for efficiency)
    bool,  // Bool
    std::shared_ptr<std::vector<Value>>,  // List (shared)
    std::shared_ptr<std::unordered_map<string_type, Value>>,  // Dict
    Function,  // User function
    NativeFunction,  // Native C++ function
    Object,  // Object instance
    Iterator  // Iterator
    >
    data_;

  // Reference counting for memory management
  mutable std::int32_t refCount_ = 0;

 public:
  Value() :
      type_(Type::NONE)
  {
  }
  Value(std::int64_t v) :
      type_(Type::INT),
      data_(v)
  {
  }
  Value(double v) :
      type_(Type::FLOAT),
      data_(v)
  {
  }
  Value(const string_type& v) :
      type_(Type::STRING),
      data_(std::make_shared<string_type>(v))
  {
  }
  Value(bool v) :
      type_(Type::BOOL),
      data_(v)
  {
  }
  Value(const std::vector<Value>& v) :
      type_(Type::LIST),
      data_(std::make_shared<std::vector<Value>>(v))
  {
  }
  Value(std::shared_ptr<std::vector<Value>> v) :
      type_(Type::LIST),
      data_(v)
  {
  }

  // Copy constructor with COW (Copy-On-Write)
  Value(const Value& other) :
      type_(other.type_),
      data_(other.data_)
  {
  }

  Type getType() const { return type_; }
  void setType(const Type type) { type_ = type; }
  void setData(const std::variant<std::monostate,  // None
    std::int64_t,  // Int
    double,  // Float
    std::shared_ptr<string_type>,  // String (shared for efficiency)
    bool,  // Bool
    std::shared_ptr<std::vector<Value>>,  // List (shared)
    std::shared_ptr<std::unordered_map<string_type, Value>>,  // Dict
    Function,  // User function
    NativeFunction,  // Native C++ function
    Object,  // Object instance
    Iterator  // Iterator
    > data)
  {
    data_ = data;
  }
  // Type checks
  bool isNone() const { return type_ == Type::NONE; }
  bool isInt() const { return type_ == Type::INT; }
  bool isFloat() const { return type_ == Type::FLOAT; }
  bool isNumber() const { return isInt() || isFloat(); }
  bool isString() const { return type_ == Type::STRING; }
  bool isBool() const { return type_ == Type::BOOL; }
  bool isList() const { return type_ == Type::LIST; }
  bool isDict() const { return type_ == Type::DICT; }
  bool isFunction() const { return type_ == Type::FUNCTION; }
  bool isCallable() const { return isFunction() || type_ == Type::NATIVE_FUNCTION; }
  bool isIterable() const { return isList() || isString() || isDict(); }
  // Getters with safety
  std::int64_t asInt() const;
  double asFloat() const;
  const string_type& asString() const;
  bool asBool() const;
  std::vector<Value>& asList();
  const std::vector<Value>& asList() const;
  std::unordered_map<string_type, Value>& asDict() const;
  Function& asFunction();
  NativeFunction& asNativeFunction();
  // Type conversions
  double toFloat() const;
  std::int64_t toInt() const;
  bool toBool() const;
  string_type toString() const;
  std::string repr() const;
  // Hash for use in dictionaries
  std::size_t hash() const;
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
  // Unary operators
  Value operator-() const;
  Value operator!() const;
  // Subscript operator
  Value getItem(const Value& key) const;
  void setItem(const Value& key, const Value& value);
  // Iterator support
  Value getIterator() const;
  bool hasNext() const;
  Value next();
};

}
}
}
