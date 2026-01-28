#include "../../../include/runtime/vm/constant_pool.hpp"


namespace mylang {
namespace runtime {

std::int32_t ConstantPool::addConstant(const object::Value& val)
{
  if (val.isInt())
  {
    std::int64_t v  = val.asInt();
    auto         it = IntConstants_.find(v);
    if (it != IntConstants_.end())
      return it->second;
    std::int32_t idx = Constants_.size();
    Constants_.push_back(val);
    IntConstants_[v] = idx;
    return idx;
  }

  if (val.isFloat())
  {
    double v  = val.asFloat();
    auto   it = FloatConstants_.find(v);
    if (it != FloatConstants_.end())
      return it->second;
    std::int32_t idx = Constants_.size();
    Constants_.push_back(val);
    FloatConstants_[v] = idx;
    return idx;
  }

  if (val.isString())
  {
    const StringRef& v  = val.asString();
    auto             it = StringConstants_.find(v);
    if (it != StringConstants_.end())
      return it->second;
    std::int32_t idx = Constants_.size();
    Constants_.push_back(val);
    StringConstants_[v] = idx;
    return idx;
  }

  // Other types - no deduplication
  std::int32_t idx = Constants_.size();
  Constants_.push_back(val);
  return idx;
}

const std::vector<object::Value>& ConstantPool::getConstants() const { return Constants_; }

void ConstantPool::optimize()
{
  // Remove unused constants (requires usage tracking)
}

}
}