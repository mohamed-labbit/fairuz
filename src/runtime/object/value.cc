#include "../../../include/runtime/object/value.hpp"
#include "../../../include/diag/diagnostic.hpp"

#include <cmath>


namespace mylang {
namespace runtime {

// Getters with safety
std::int64_t object::Value::asInt() const
{
  if (!isInt())
    diagnostic::engine.panic("Value is not an integer");
  return std::get<std::int64_t>(Data_);
}

double object::Value::asFloat() const
{
  if (!isFloat())
    diagnostic::engine.panic("Value is not a float");
  return std::get<double>(Data_);
}

const StringRef& object::Value::asString() const
{
  if (!isString())
    diagnostic::engine.panic("Value is not a string");
  return std::get<StringRef>(Data_);
}

bool object::Value::asBool() const
{
  if (!isBool())
    diagnostic::engine.panic("Value is not a boolean");
  return std::get<bool>(Data_);
}

std::vector<object::Value>& object::Value::asList()
{
  if (!isList())
    diagnostic::engine.panic("Value is not a list");
  return std::get<std::vector<Value>>(Data_);
}

const std::vector<object::Value>& object::Value::asList() const
{
  if (!isList())
    diagnostic::engine.panic("Value is not a list");
  return std::get<std::vector<Value>>(Data_);
}

std::unordered_map<StringRef, object::Value, StringRefHash, StringRefEqual>& object::Value::asDict()
{
  if (!isDict())
    diagnostic::engine.panic("Value is not a dict");
  return std::get<std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>>(Data_);
}

const std::unordered_map<StringRef, object::Value, StringRefHash, StringRefEqual>& object::Value::asDict() const
{
  if (!isDict())
    diagnostic::engine.panic("Value is not a dict");
  return std::get<std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>>(Data_);
}

typename object::Value::Function& object::Value::asFunction()
{
  if (!isFunction())
    diagnostic::engine.panic("Value is not a function");
  return std::get<Function>(Data_);
}

typename object::Value::NativeFunction& object::Value::asNativeFunction()
{
  if (Type_ != Type::NATIVE_FUNCTION)
    diagnostic::engine.panic("Not a native function");
  return std::get<NativeFunction>(Data_);
}

// Type conversions
double object::Value::toFloat() const
{
  if (isInt())
    return static_cast<double>(asInt());
  if (isFloat())
    return asFloat();
  if (isBool())
    return asBool() ? 1.0 : 0.0;
  diagnostic::engine.panic("Cannot convert to float");
}

std::int64_t object::Value::toInt() const
{
  if (isInt())
    return asInt();
  if (isFloat())
    return static_cast<std::int64_t>(asFloat());
  if (isBool())
    return asBool() ? 1 : 0;
  if (isString())
    return std::stoll(asString().toUtf8());
  diagnostic::engine.panic("Cannot convert to int");
}

bool object::Value::toBool() const
{
  switch (Type_)
  {
  case Type::NONE : return false;
  case Type::INT : return asInt() != 0;
  case Type::FLOAT : return asFloat() != 0.0;
  case Type::STRING : return !asString().empty();
  case Type::BOOL : return asBool();
  case Type::LIST : return !asList().empty();
  case Type::DICT : return !(asDict().empty());
  default : return true;
  }
}

StringRef object::Value::toString() const
{
  StringRef ret;

  switch (Type_)
  {
  case Type::NONE : return u"None";
  case Type::INT : return ret.fromUtf8(std::to_string(asInt()));
  case Type::FLOAT : {
    StringRef s;
    s = s.fromUtf8(std::to_string(asFloat()));
    for (SizeType i = s.len() - 1; i > 0; --i)
    {
      if (s[i] == u'0')
        s.erase(i);
      else
      {
        if (s[i] == '.')
          s += u'0';
        break;
      }
    }
    return s;
  }
  case Type::STRING : return asString();
  case Type::BOOL : return asBool() ? u"True" : u"False";
  case Type::LIST : {
    StringRef                         result = u"[";
    const std::vector<object::Value>& list   = asList();
    for (SizeType i = 0; i < list.size(); ++i)
    {
      result += list[i].toString();
      if (i + 1 < list.size())
        result += u", ";
    }
    return result + u"]";
  }
  case Type::DICT : {
    StringRef                                                                  result = u"{";
    const std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>& dict =
      std::get<std::unordered_map<StringRef, Value, StringRefHash, StringRefEqual>>(Data_);
    SizeType count = 0;
    for (const auto& [k, v] : dict)
    {
      result += u"'" + k + u"': " + v.toString();
      if (++count < dict.size())
        result += u", ";
    }
    return result + u"}";
  }
  case Type::FUNCTION : return u"<function>";
  case Type::NATIVE_FUNCTION : return u"<native function>";
  default : return u"<object>";
  }
}

std::string object::Value::repr() const
{
  if (isString())
    return "'" + asString().toUtf8() + "'";
  return toString().toUtf8();
}

// Hash for use in dictionaries
SizeType object::Value::hash() const
{
  std::hash<StringRef> hasher;
  return hasher(toString());
}

// Comparison operators
bool object::Value::operator==(const Value& other) const
{
  if (Type_ != other.Type_)
  {
    // Handle numeric comparisons
    if (isNumber() && other.isNumber())
      return toFloat() == other.toFloat();
    return false;
  }

  switch (Type_)
  {
  case Type::NONE : return true;
  case Type::INT : return asInt() == other.asInt();
  case Type::FLOAT : return asFloat() == other.asFloat();
  case Type::STRING : return asString() == other.asString();
  case Type::BOOL : return asBool() == other.asBool();
  case Type::LIST : return asList() == other.asList();
  default : return false;
  }
}

bool object::Value::operator<(const Value& other) const
{
  if (isNumber() && other.isNumber())
    return toFloat() < other.toFloat();

  // if (isString() && other.isString())
  // {
  //   return asString() < other.asString();
  // }
  diagnostic::engine.panic("Cannot compare types");
}

bool object::Value::operator>(const Value& other) const { return other < *this; }
bool object::Value::operator<=(const Value& other) const { return !(other < *this); }
bool object::Value::operator>=(const Value& other) const { return !(*this < other); }
bool object::Value::operator!=(const Value& other) const { return !(*this == other); }

// Arithmetic operators with type promotion
object::Value object::Value::operator+(const Value& other) const
{
  if (isInt() && other.isInt())
    return Value(asInt() + other.asInt());
  if (isNumber() && other.isNumber())
    return Value(toFloat() + other.toFloat());
  if (isString() || other.isString())
    return Value(toString() + other.toString());
  if (isList() && other.isList())
  {
    std::vector<object::Value>        result    = asList();
    const std::vector<object::Value>& otherList = other.asList();
    result.insert(result.end(), otherList.begin(), otherList.end());
    return Value(result);
  }
  diagnostic::engine.panic("Unsupported operand types for +");
}

object::Value object::Value::operator-(const Value& other) const
{
  if (isInt() && other.isInt())
    return Value(asInt() - other.asInt());
  if (isNumber() && other.isNumber())
    return Value(toFloat() - other.toFloat());
  diagnostic::engine.panic("Unsupported operand types for -");
}

object::Value object::Value::operator*(const Value& other) const
{
  if (isInt() && other.isInt())
    return Value(asInt() * other.asInt());
  if (isNumber() && other.isNumber())
    return Value(toFloat() * other.toFloat());
  // String/List repetition
  if (isString() && other.isInt())
  {
    StringRef result;
    for (std::int64_t i = 0; i < other.asInt(); ++i)
      result += asString();
    return Value(result);
  }
  if (isList() && other.isInt())
  {
    std::vector<Value> result;
    for (std::int64_t i = 0; i < other.asInt(); ++i)
      result.insert(result.end(), asList().begin(), asList().end());
    return Value(result);
  }
  diagnostic::engine.panic("Unsupported operand types for *");
}

object::Value object::Value::operator/(const Value& other) const
{
  if (isNumber() && other.isNumber())
  {
    double divisor = other.toFloat();
    if (divisor == 0.0)
      diagnostic::engine.panic("Division by zero");
    return Value(toFloat() / divisor);
  }
  diagnostic::engine.panic("Unsupported operand types for /");
}

object::Value object::Value::operator%(const Value& other) const
{
  if (isInt() && other.isInt())
  {
    if (other.asInt() == 0)
      diagnostic::engine.panic("Modulo by zero");
    return Value(asInt() % other.asInt());
  }
  if (isNumber() && other.isNumber())
    return Value(std::fmod(toFloat(), other.toFloat()));
  diagnostic::engine.panic("Unsupported operand types for %");
}

object::Value object::Value::pow(const Value& other) const
{
  if (isNumber() && other.isNumber())
    return Value(std::pow(toFloat(), other.toFloat()));
  diagnostic::engine.panic("Unsupported operand types for **");
}

// Unary operators
object::Value object::Value::operator-() const
{
  if (isInt())
    return Value(-asInt());
  if (isFloat())
    return Value(-asFloat());
  diagnostic::engine.panic("Unsupported operand type for unary -");
}

object::Value object::Value::operator!() const { return Value(!toBool()); }

// Subscript operator
object::Value object::Value::getItem(const Value& key) const
{
  if (isList())
  {
    std::int64_t                      index = key.toInt();
    const std::vector<object::Value>& list  = asList();
    if (index < 0)
      index += list.size();
    if (index < 0 || index >= list.size())
      diagnostic::engine.panic("List index out of range");
    return list[index];
  }
  if (isDict())
  {
    const std::unordered_map<StringRef, object::Value, StringRefHash, StringRefEqual>& dict = asDict();
    auto                                                                               it   = dict.find(key.toString());
    if (it == dict.end())
      diagnostic::engine.panic("Key not found: " /*+ key.toString()*/);
    return it->second;
  }
  if (isString())
  {
    std::int64_t     index = key.toInt();
    const StringRef& str   = asString();
    if (index < 0)
      index += str.len();
    if (index < 0 || index >= str.len())
      diagnostic::engine.panic("String index out of range");
    return Value(StringRef(str[index]));
  }
  diagnostic::engine.panic("Object is not subscriptable");
}

void object::Value::setItem(const Value& key, const Value& value)
{
  if (isList())
  {
    std::int64_t                index = key.toInt();
    std::vector<object::Value>& list  = asList();
    if (index < 0)
      index += list.size();
    if (index < 0 || index >= list.size())
      diagnostic::engine.panic("List index out of range");
    list[index] = value;
  }
  else if (isDict())
    asDict()[key.toString()] = value;
  else
    diagnostic::engine.panic("Object does not support item assignment");
}

// Iterator support
object::Value object::Value::getIterator() const
{
  if (isList())
  {
    Iterator it;
    it.items = std::get<std::vector<Value>>(Data_);
    it.index = 0;
    Value result;
    result.Type_ = Type::ITERATOR;
    result.Data_ = it;
    return result;
  }
  diagnostic::engine.panic("Object is not iterable");
}

bool object::Value::hasNext() const
{
  if (Type_ == Type::ITERATOR)
  {
    const Iterator& it = std::get<Iterator>(Data_);
    return it.index < it.items.size();
  }
  return false;
}

object::Value object::Value::next()
{
  if (Type_ == Type::ITERATOR)
  {
    Iterator& it = std::get<Iterator>(Data_);
    if (it.index >= it.items.size())
      diagnostic::engine.panic("Iterator exhausted");
    return it.items[it.index++];
  }
  diagnostic::engine.panic("Object is not an iterator");
}

}
}