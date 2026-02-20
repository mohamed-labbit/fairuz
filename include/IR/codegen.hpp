#pragma once

#include "../ast/ast.hpp"
#include "env.hpp"
#include "value.hpp"

namespace mylang {
namespace IR {

class CodeGenerator {
public:
    CodeGenerator(Environment* env)
        : Env_(env)
    {
    }

    Value eval(ast::ASTNode const* node);

private:
    Environment* Env_;

    Value callUserFunction(StringRef const& func_name, std::vector<Value> const& args);
};

}
}
