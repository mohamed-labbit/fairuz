#pragma once

#include "../runtime/object/value.hpp"
#include <memory>
#include <stdexcept>
#include <unordered_map>


namespace mylang {
namespace runtime {

class Environment
{
 private:
  std::unordered_map<std::string, object::Value> variables_;
  std::shared_ptr<Environment>                   parent_;  // For nested scopes

 public:
  Environment() :
      parent_(nullptr)
  {
  }

  explicit Environment(std::shared_ptr<Environment> parent) :
      parent_(parent)
  {
  }

  // Define a variable in current scope
  void define(const std::string& name, const object::Value& value) { variables_[name] = value; }

  // Get a variable (searches parent scopes)
  object::Value get(const std::string& name) const
  {
    auto it = variables_.find(name);
    if (it != variables_.end())
      return it->second;

    if (parent_)
      return parent_->get(name);

    throw std::runtime_error("Undefined variable: " + name);
  }

  // Assign to existing variable (searches parent scopes)
  void assign(const std::string& name, const object::Value& value)
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

    throw std::runtime_error("Undefined variable: " + name);
  }

  bool exists(const std::string& name) const
  {
    if (variables_.find(name) != variables_.end())
      return true;
    return parent_ && parent_->exists(name);
  }
};

}  // namespace runtime
}  // namespace mylang