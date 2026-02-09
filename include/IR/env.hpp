#pragma once

#include "value.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>


namespace mylang {
namespace IR {

class Environment
{
 private:
  std::unordered_map<StringRef, Value> variables_;
  Environment*                         parent_;  // For nested scopes

 public:
  Environment() :
      parent_(nullptr)
  {
  }

  explicit Environment(Environment* parent) :
      parent_(parent)
  {
  }

  // Define a variable in current scope
  void define(const StringRef& name, const Value& value) { variables_[name] = value; }

  // Get a variable (searches parent scopes)
  Value get(const StringRef& name) const
  {
    auto it = variables_.find(name);
    if (it != variables_.end())
      return it->second;

    if (parent_)
      return parent_->get(name);

    throw std::runtime_error("Undefined variable: " + name.toUtf8());
  }

  // Assign to existing variable (searches parent scopes)
  void assign(const StringRef& name, const Value& value)
  {
    auto it = variables_.find(name);
    if (it != variables_.end())
    {
      it->second = value;
      return;
    }

    if (parent_)
    {
      parent_->assign(name, value);
      return;
    }

    throw std::runtime_error("Undefined variable: " + name.toUtf8());
  }

  bool exists(const StringRef& name) const
  {
    if (variables_.find(name) != variables_.end())
      return true;
    return parent_ && parent_->exists(name);
  }
};

}  // namespace runtime
}  // namespace mylang