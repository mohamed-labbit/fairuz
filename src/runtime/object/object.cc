#include "../../../include/runtime/object/object.hpp"
#include "../../../include/diag/diagnostic.hpp"

#include <cmath>


namespace mylang {
namespace runtime {

// Getters with safety
std::int64_t object::Value::asInt() const
{
  if (!isInt()) diagnostic::engine.panic("Value is not an integer");
  return std::get<std::int64_t>(data_);
}

double object::Value::asFloat() const
{
  if (!isFloat()) diagnostic::engine.panic("Value is not a float");
  return std::get<double>(data_);
}

const string_type& object::Value::asString() const
{
  if (!isString()) diagnostic::engine.panic("Value is not a string");
  return *std::get<std::shared_ptr<string_type>>(data_);
}

bool object::Value::asBool() const
{
  if (!isBool()) diagnostic::engine.panic("Value is not a boolean");
  return std::get<bool>(data_);
}

std::vector<object::Value>& object::Value::asList()
{
  if (!isList()) diagnostic::engine.panic("Value is not a list");
  return *std::get<std::shared_ptr<std::vector<Value>>>(data_);
}

const std::vector<object::Value>& object::Value::asList() const
{
  if (!isList()) diagnostic::engine.panic("Value is not a list");
  return *std::get<std::shared_ptr<std::vector<Value>>>(data_);
}

std::unordered_map<string_type, object::Value>& object::Value::asDict() const
{
  if (!isDict()) diagnostic::engine.panic("Value is not a dict");
  return *std::get<std::shared_ptr<std::unordered_map<string_type, Value>>>(data_);
}

typename object::Value::Function& object::Value::asFunction()
{
  if (!isFunction()) diagnostic::engine.panic("Value is not a function");
  return std::get<Function>(data_);
}

typename object::Value::NativeFunction& object::Value::asNativeFunction()
{
  if (type_ != Type::NATIVE_FUNCTION) diagnostic::engine.panic("Not a native function");
  return std::get<NativeFunction>(data_);
}

// Type conversions
double object::Value::toFloat() const
{
  if (isInt()) return static_cast<double>(asInt());
  if (isFloat()) return asFloat();
  if (isBool()) return asBool() ? 1.0 : 0.0;
  diagnostic::engine.panic("Cannot convert to float");
}

std::int64_t object::Value::toInt() const
{
  if (isInt()) return asInt();
  if (isFloat()) return static_cast<std::int64_t>(asFloat());
  if (isBool()) return asBool() ? 1 : 0;
  if (isString()) return std::stoll(utf8::utf16to8(asString()));
  diagnostic::engine.panic("Cannot convert to int");
}

bool object::Value::toBool() const
{
  switch (type_)
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

string_type object::Value::toString() const
{
  switch (type_)
  {
  case Type::NONE : return u"None";
  case Type::INT : return utf8::utf8to16(std::to_string(asInt()));
  case Type::FLOAT : {
    auto s = utf8::utf8to16(std::to_string(asFloat()));
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s += u"0";
    return s;
  }
  case Type::STRING : return asString();
  case Type::BOOL : return asBool() ? u"True" : u"False";
  case Type::LIST : {
    string_type result = u"[";
    const auto& list = asList();
    for (std::size_t i = 0; i < list.size(); i++)
    {
      result += list[i].toString();
      if (i + 1 < list.size()) result += u", ";
    }
    return result + u"]";
  }
  case Type::DICT : {
    string_type result = u"{";
    const auto& dict = std::get<std::shared_ptr<std::unordered_map<string_type, Value>>>(data_);
    std::size_t count = 0;
    for (const auto& [k, v] : *dict)
    {
      result += u"'" + k + u"': " + v.toString();
      if (++count < dict->size()) result += u", ";
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
  if (isString()) return "'" + utf8::utf16to8(asString()) + "'";
  return utf8::utf16to8(toString());
}

// Hash for use in dictionaries
std::size_t object::Value::hash() const
{
  std::hash<string_type> hasher;
  return hasher(toString());
}

// Comparison operators
bool object::Value::operator==(const Value& other) const
{
  if (type_ != other.type_)
  {
    // Handle numeric comparisons
    if (isNumber() && other.isNumber()) return toFloat() == other.toFloat();
    return false;
  }

  switch (type_)
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
  if (isNumber() && other.isNumber()) return toFloat() < other.toFloat();
  if (isString() && other.isString()) return asString() < other.asString();
  diagnostic::engine.panic("Cannot compare types");
}

bool object::Value::operator>(const Value& other) const { return other < *this; }
bool object::Value::operator<=(const Value& other) const { return !(other < *this); }
bool object::Value::operator>=(const Value& other) const { return !(*this < other); }
bool object::Value::operator!=(const Value& other) const { return !(*this == other); }

// Arithmetic operators with type promotion
object::Value object::Value::operator+(const Value& other) const
{
  if (isInt() && other.isInt()) return Value(asInt() + other.asInt());
  if (isNumber() && other.isNumber()) return Value(toFloat() + other.toFloat());
  if (isString() || other.isString()) return Value(toString() + other.toString());
  if (isList() && other.isList())
  {
    auto result = asList();
    const auto& otherList = other.asList();
    result.insert(result.end(), otherList.begin(), otherList.end());
    return Value(result);
  }
  diagnostic::engine.panic("Unsupported operand types for +");
}

object::Value object::Value::operator-(const Value& other) const
{
  if (isInt() && other.isInt()) return Value(asInt() - other.asInt());
  if (isNumber() && other.isNumber()) return Value(toFloat() - other.toFloat());
  diagnostic::engine.panic("Unsupported operand types for -");
}

object::Value object::Value::operator*(const Value& other) const
{
  if (isInt() && other.isInt()) return Value(asInt() * other.asInt());
  if (isNumber() && other.isNumber()) return Value(toFloat() * other.toFloat());
  // String/List repetition
  if (isString() && other.isInt())
  {
    string_type result;
    for (std::int64_t i = 0; i < other.asInt(); i++) result += asString();
    return Value(result);
  }
  if (isList() && other.isInt())
  {
    std::vector<Value> result;
    for (std::int64_t i = 0; i < other.asInt(); i++) result.insert(result.end(), asList().begin(), asList().end());
    return Value(result);
  }
  diagnostic::engine.panic("Unsupported operand types for *");
}

object::Value object::Value::operator/(const Value& other) const
{
  if (isNumber() && other.isNumber())
  {
    double divisor = other.toFloat();
    if (divisor == 0.0) diagnostic::engine.panic("Division by zero");
    return Value(toFloat() / divisor);
  }
  diagnostic::engine.panic("Unsupported operand types for /");
}

object::Value object::Value::operator%(const Value& other) const
{
  if (isInt() && other.isInt())
  {
    if (other.asInt() == 0) diagnostic::engine.panic("Modulo by zero");
    return Value(asInt() % other.asInt());
  }
  if (isNumber() && other.isNumber()) return Value(std::fmod(toFloat(), other.toFloat()));
  diagnostic::engine.panic("Unsupported operand types for %");
}

object::Value object::Value::pow(const Value& other) const
{
  if (isNumber() && other.isNumber()) return Value(std::pow(toFloat(), other.toFloat()));
  diagnostic::engine.panic("Unsupported operand types for **");
}

// Unary operators
object::Value object::Value::operator-() const
{
  if (isInt()) return Value(-asInt());
  if (isFloat()) return Value(-asFloat());
  diagnostic::engine.panic("Unsupported operand type for unary -");
}

object::Value object::Value::operator!() const { return Value(!toBool()); }

// Subscript operator
object::Value object::Value::getItem(const Value& key) const
{
  if (isList())
  {
    std::int64_t index = key.toInt();
    const auto& list = asList();
    if (index < 0) index += list.size();
    if (index < 0 || index >= list.size()) diagnostic::engine.panic("List index out of range");
    return list[index];
  }
  if (isDict())
  {
    const auto& dict = asDict();
    auto it = dict.find(key.toString());
    if (it == dict.end()) diagnostic::engine.panic("Key not found: " + utf8::utf16to8(key.toString()));
    return it->second;
  }
  if (isString())
  {
    std::int64_t index = key.toInt();
    const auto& str = asString();
    if (index < 0) index += str.size();
    if (index < 0 || index >= str.size()) diagnostic::engine.panic("String index out of range");
    return Value(string_type(1, str[index]));
  }
  diagnostic::engine.panic("Object is not subscriptable");
}

void object::Value::setItem(const Value& key, const Value& value)
{
  if (isList())
  {
    std::int64_t index = key.toInt();
    auto& list = asList();
    if (index < 0) index += list.size();
    if (index < 0 || index >= list.size()) diagnostic::engine.panic("List index out of range");
    list[index] = value;
  }
  else if (isDict()) { asDict()[key.toString()] = value; }
  else
  {
    diagnostic::engine.panic("Object does not support item assignment");
  }
}

// Iterator support
object::Value object::Value::getIterator() const
{
  if (isList())
  {
    Iterator it;
    it.items = std::get<std::shared_ptr<std::vector<Value>>>(data_);
    it.index = 0;
    Value result;
    result.type_ = Type::ITERATOR;
    result.data_ = it;
    return result;
  }
  diagnostic::engine.panic("Object is not iterable");
}

bool object::Value::hasNext() const
{
  if (type_ == Type::ITERATOR)
  {
    const auto& it = std::get<Iterator>(data_);
    return it.index < it.items->size();
  }
  return false;
}

object::Value object::Value::next()
{
  if (type_ == Type::ITERATOR)
  {
    auto& it = std::get<Iterator>(data_);
    if (it.index >= it.items->size()) diagnostic::engine.panic("Iterator exhausted");
    return (*it.items)[it.index++];
  }
  diagnostic::engine.panic("Object is not an iterator");
}

}
}