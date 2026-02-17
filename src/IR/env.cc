#include "../../include/IR/env.hpp"

namespace mylang {
namespace IR {

Value Environment::get(StringRef const& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end())
        return it->second;

    if (parent_)
        return parent_->get(name);

    throw std::runtime_error("Undefined variable: " + std::string(name.data()));
}

void Environment::assign(StringRef const& name, Value const& value)
{
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        it->second = value;
        return;
    }

    if (parent_) {
        parent_->assign(name, value);
        return;
    }

    throw std::runtime_error("Undefined variable: " + std::string(name.data()));
}

}
}
