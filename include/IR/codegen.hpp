#pragma once

#include "../ast/ast.hpp"
#include "env.hpp"
#include "value.hpp"

namespace mylang {
namespace IR {

class CodeGenerator {
public:
    Value eval(ast::ASTNode const* node);

private:
    Environment* Env_;

    Value callUserFunction(Value& func_value, std::vector<Value> const& args);
};

}
}
