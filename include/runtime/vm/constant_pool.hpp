#pragma once

#include "../../macros.hpp"
#include "../object/object.hpp"
#include <unordered_map>
#include <vector>


// ============================================================================
// CONSTANT POOL - Deduplicated constants
// ============================================================================

namespace mylang {
namespace runtime {

class ConstantPool
{
 private:
  std::vector<object::Value>                     Constants_;
  std::unordered_map<StringType, std::int32_t>   StringConstants_;
  std::unordered_map<std::int64_t, std::int32_t> IntConstants_;
  std::unordered_map<double, std::int32_t>       FloatConstants_;

 public:
  std::int32_t addConstant(const object::Value& val);

  const std::vector<object::Value>& getConstants() const;

  void optimize();
};

}
}