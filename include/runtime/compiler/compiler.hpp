#pragma once

#include "../../IR/value.hpp"
#include "../opcode/chunk.hpp"
#include "../opcode/opcode.hpp"
#include "../../ast/ast.hpp"

class ASTNode;

namespace mylang {
namespace runtime {

class Compiler {
public:
    void compile(const ast::ASTNode* node);

private:
    OpCodeChunk CurrentChunk_;

    void emitConstant(IR::Value const value)
    {
        uint8_t index = CurrentChunk_.addConstant(value);
        CurrentChunk_.push(static_cast<uint8_t>(OpCode::OP_CONST));
        CurrentChunk_.push(index);
    }
};

}
}
