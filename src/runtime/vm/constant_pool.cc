#include "../../../include/runtime/vm/constant_pool.hpp"


namespace mylang {
namespace runtime {

std::int32_t ConstantPool::addConstant(const object::Value& val)
{
  if (val.isInt())
  {
    std::int64_t v = val.asInt();
    auto it = intConstants_.find(v);
    if (it != intConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    intConstants_[v] = idx;
    return idx;
  }

  if (val.isFloat())
  {
    double v = val.asFloat();
    auto it = floatConstants_.find(v);
    if (it != floatConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    floatConstants_[v] = idx;
    return idx;
  }

  if (val.isString())
  {
    const string_type& v = val.asString();
    auto it = stringConstants_.find(v);
    if (it != stringConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    stringConstants_[v] = idx;
    return idx;
  }

  // Other types - no deduplication
  std::int32_t idx = constants_.size();
  constants_.push_back(val);
  return idx;
}

const std::vector<object::Value>& ConstantPool::getConstants() const { return constants_; }

void ConstantPool::optimize()
{
  // Remove unused constants (requires usage tracking)
}

}
}