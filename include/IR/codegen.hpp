#pragma once

#include "value.hpp"
#include "env.hpp"
#include "../ast/ast.hpp"

#include <builtins.h>
#include <functional>

namespace mylang {
namespace IR {

using namespace runtime;

class CodeGenerator {
public:
    Value eval(ast::ASTNode const* node);

private:
    Environment* Env_;

    Value callUserFunction(Value& func_value, std::vector<Value> const& args);
};

}
}
